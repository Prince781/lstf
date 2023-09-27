#include "lstf-vm-program.h"
#include "data-structures/ptr-hashmap.h"
#include "io/outputstream.h"
#include "lstf-vm-opcodes.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

lstf_vm_program *lstf_vm_program_ref(lstf_vm_program *prog)
{
    if (!prog)
        return NULL;

    assert(prog->floating || prog->refcount > 0);

    if (prog->floating) {
        prog->floating = false;
        prog->refcount = 1;
    } else {
        prog->refcount++;
    }

    return prog;
}

static void lstf_vm_program_destroy(lstf_vm_program *prog)
{
    if (!prog)
        return;

    assert(prog->floating || prog->refcount == 0);

    free(prog->debuginfo);
    ptr_hashmap_destroy(prog->debug_entries);
    ptr_hashmap_destroy(prog->debug_symbols);
    free(prog->data);
    free(prog->code);
    free(prog);
}

void lstf_vm_program_unref(lstf_vm_program *prog)
{
    if (!prog)
        return;

    assert(prog->floating || prog->refcount > 0);

    if (prog->floating || --prog->refcount == 0)
        lstf_vm_program_destroy(prog);
}

// TODO: reuse code from lstf-virtualmachine.c ?

static const char *
format_i64_hex(int64_t i)
{
    static char buffer[256];
    if (i >= 0) {
        snprintf(buffer, sizeof buffer - 1, "%#"PRIx64, (uint64_t)i);
    } else {
        snprintf(buffer, sizeof buffer - 1, "-%#"PRIx64, (uint64_t)-i);
    }
    return buffer;
}

static bool
lstf_vm_program_read_imm_u8(lstf_vm_program *prog,
                            uint64_t        *offset,
                            uint8_t         *byte)
{
    if (*offset >= prog->code_size)
        return false;
    *byte = prog->code[(*offset)++];
    return true;
}

static bool
lstf_vm_program_read_imm_i8(lstf_vm_program *prog,
                            uint64_t        *offset,
                            int8_t          *byte)
{
    uint8_t value = 0;

    if (!lstf_vm_program_read_imm_u8(prog, offset, &value))
        return false;

    *byte = value;
    return true;
}

static bool
lstf_vm_program_read_imm_bool(lstf_vm_program *prog,
                              uint64_t        *offset,
                              bool            *boolean)
{
    uint8_t byte = 0;

    if (!lstf_vm_program_read_imm_u8(prog, offset, &byte))
        return false;

    *boolean = byte != 0;
    return true;
}

static bool
lstf_vm_program_read_imm_u64(lstf_vm_program *prog,
                             uint64_t        *offset,
                             uint64_t        *integer)
{
    uint64_t value = 0;

    // we read in an integer in most-significant byte first (big endian / network byte order)
    for (unsigned i = 0; i < sizeof(value); i++) {
        uint8_t byte;
        if (!lstf_vm_program_read_imm_u8(prog, offset, &byte))
            return false;
        value |= ((uint64_t)byte) << ((sizeof(value) - 1 - i) * CHAR_BIT);
    }

    if (integer)
        *integer = value;

    return true;
}

static bool
lstf_vm_program_read_imm_i64(lstf_vm_program *prog,
                             uint64_t        *offset,
                             int64_t         *integer)
{
    uint64_t value;

    if (!lstf_vm_program_read_imm_u64(prog, offset, &value))
        return false;

    if (integer)
        *integer = (int64_t)value;
    return true;
}

static bool
lstf_vm_program_read_imm_string(lstf_vm_program *prog,
                                uint64_t        *offset,
                                char           **expression_string)
{
    char *string_beginning = (char *)(prog->code + *offset);
    uint8_t byte;
    bool status;

    while ((status = lstf_vm_program_read_imm_u8(prog, offset, &byte))) {
        if (byte == '\0')
            break;
    }

    if (expression_string && status)
        *expression_string = string_beginning;

    return status;
}

// computes round_up(log_16(v))
static inline uint8_t log16i(uint64_t v) {
    uint8_t l = 0;
    bool rem = false;
    for (; v; l++) {
        rem |= v & 0x7;
        v >>= 4;
    }
    return l + rem;
}

bool lstf_vm_program_disassemble(lstf_vm_program *prog, outputstream *ostream, uint8_t *pc, ptr_list *offsets)
{
    uint8_t opcode = 0;
    uint64_t offset = 0;
    uint8_t field_width = log16i(prog->code_size) + 1;

    while (lstf_vm_program_read_imm_u8(prog, &offset, &opcode)) {
        // print header before first instruction of function body
        if (opcode == lstf_vm_op_params) {
            if (offset > 1 && !outputstream_printf(ostream, "\n"))
                goto err_write;
            if (!outputstream_printf(ostream, "<%#0*"PRIx64"> function", field_width, offset - 1))
                goto err_write;
            if (offset - 1 == (uint64_t)(prog->entry_point - prog->code) && !outputstream_printf(ostream, " (entry)"))
                goto err_write;
            if (!outputstream_printf(ostream, "\n"))
                goto err_write;
        }

        if (!outputstream_printf(ostream, (pc && offset - 1 == (uint64_t)(pc - prog->code)) ? " => " : "    "))
            goto err_write;
        if (!outputstream_printf(ostream, "<%#0*"PRIx64"> ", field_width, offset - 1))
            goto err_write;

        if (!lstf_vm_opcode_can_cast(opcode)) {
            if (!outputstream_printf(ostream, "<invalid opcode %#hhx>\n", opcode))
                goto err_write;
        } else {
            // save PCs after the original pc
            if (pc && offsets && offset-1 > (uint64_t)(pc - prog->code))
                ptr_list_append(offsets, prog->code + offset-1);

            switch (opcode) {
            case lstf_vm_op_load_frameoffset:
            {
                int64_t fp_offset;
                if (!lstf_vm_program_read_imm_i64(prog, &offset, &fp_offset))
                    goto err_read;
                if (!outputstream_printf(ostream, "load [fp + %s]\n", format_i64_hex(fp_offset)))
                    goto err_write;
            }   break;

            case lstf_vm_op_load_dataoffset:
            {
                uint64_t data_offset;
                if (!lstf_vm_program_read_imm_u64(prog, &offset, &data_offset))
                    goto err_read;
                if (!outputstream_printf(ostream, "load string [data + %#0"PRIx64"]\n", data_offset))
                    goto err_write;
            }   break;

            case lstf_vm_op_load_codeoffset:
            {
                uint64_t code_offset;
                if (!lstf_vm_program_read_imm_u64(prog, &offset, &code_offset))
                    goto err_read;
                if (!outputstream_printf(ostream, "load funptr <%#0*"PRIx64">\n", field_width, code_offset))
                    goto err_write;
            }   break;

            case lstf_vm_op_load_expression:
            {
                char *json_string = NULL;
                if (!lstf_vm_program_read_imm_string(prog, &offset, &json_string))
                    goto err_read;
                if (!outputstream_printf(ostream, "load json %s\n", json_string))
                    goto err_write;
            }   break;

            case lstf_vm_op_store:
            {
                int64_t fp_offset;
                if (!lstf_vm_program_read_imm_i64(prog, &offset, &fp_offset))
                    goto err_read;
                if (!outputstream_printf(ostream, "store [fp + %s]\n", format_i64_hex(fp_offset)))
                    goto err_write;
            }   break;

            // these are opcodes without any immediate arguments
            case lstf_vm_op_pop:
            case lstf_vm_op_set:
            case lstf_vm_op_get:
            case lstf_vm_op_append:
            case lstf_vm_op_in:
            case lstf_vm_op_calli:
            case lstf_vm_op_return:
            case lstf_vm_op_bool:
            case lstf_vm_op_land:
            case lstf_vm_op_lor:
            case lstf_vm_op_lnot:
            case lstf_vm_op_lessthan:
            case lstf_vm_op_lessthan_equal:
            case lstf_vm_op_equal:
            case lstf_vm_op_greaterthan:
            case lstf_vm_op_greaterthan_equal:
            case lstf_vm_op_add:
            case lstf_vm_op_sub:
            case lstf_vm_op_mul:
            case lstf_vm_op_div:
            case lstf_vm_op_pow:
            case lstf_vm_op_mod:
            case lstf_vm_op_neg:
            case lstf_vm_op_and:
            case lstf_vm_op_or:
            case lstf_vm_op_xor:
            case lstf_vm_op_lshift:
            case lstf_vm_op_rshift:
            case lstf_vm_op_not:
            case lstf_vm_op_print:
            case lstf_vm_op_getopt:
            case lstf_vm_op_assert:
                if (!outputstream_printf(ostream, "%s\n", lstf_vm_opcode_to_string(opcode)))
                    goto err_write;
                break;

            case lstf_vm_op_params:
            {
                uint8_t num_params;
                if (!lstf_vm_program_read_imm_u8(prog, &offset, &num_params))
                    goto err_read;
                if (!outputstream_printf(ostream, "params %hhd\n", num_params))
                    goto err_write;
            }   break;

            case lstf_vm_op_call:
            {
                uint64_t code_offset;
                if (!lstf_vm_program_read_imm_u64(prog, &offset, &code_offset))
                    goto err_read;
                if (!outputstream_printf(ostream, "call <%#0*"PRIx64">\n", field_width, code_offset))
                    goto err_write;
            }   break;

            case lstf_vm_op_schedule:
            {
                uint64_t code_offset;
                uint8_t num_params;
                if (!lstf_vm_program_read_imm_u64(prog, &offset, &code_offset))
                    goto err_read;
                if (!lstf_vm_program_read_imm_u8(prog, &offset, &num_params))
                    goto err_read;
                if (!outputstream_printf(ostream, "schedule <%#"PRIx64"> (%hhu params)\n", code_offset, num_params))
                    goto err_write;
            }   break;

            case lstf_vm_op_schedulei:
            {
                uint8_t num_params;
                if (!lstf_vm_program_read_imm_u8(prog, &offset, &num_params))
                    goto err_read;
                if (!outputstream_printf(ostream, "schedulei (%hhu params)\n", num_params))
                    goto err_write;
            }   break;

            case lstf_vm_op_closure:
            {
                uint8_t num_values;
                uint64_t code_offset;
                if (!lstf_vm_program_read_imm_u8(prog, &offset, &num_values))
                    goto err_read;
                if (!lstf_vm_program_read_imm_u64(prog, &offset, &code_offset))
                    goto err_read;
                if (!outputstream_printf(ostream, "closure <%#"PRIx64">", code_offset))
                    goto err_write;
                for (uint8_t i = 0; i < num_values; i++) {
                    bool is_local;

                    if (!lstf_vm_program_read_imm_bool(prog, &offset, &is_local))
                        goto err_read;

                    if (is_local) {
                        int64_t fp_offset;
                        if (!lstf_vm_program_read_imm_i64(prog, &offset, &fp_offset))
                            goto err_read;
                        if (!outputstream_printf(ostream, ", [fp + %s]", format_i64_hex(fp_offset)))
                            goto err_write;
                    } else {
                        uint64_t upvalue_id;
                        if (!lstf_vm_program_read_imm_u64(prog, &offset, &upvalue_id))
                            goto err_read;
                        if (!outputstream_printf(ostream, ", up#%"PRIu64, upvalue_id))
                            goto err_write;
                    }
                }
                if (!outputstream_printf(ostream, "\n"))
                    goto err_write;
            }   break;

            case lstf_vm_op_upget:
            case lstf_vm_op_upset:
            {
                uint8_t upvalue_id;
                if (!lstf_vm_program_read_imm_u8(prog, &offset, &upvalue_id))
                    goto err_read;
                if (!outputstream_printf(ostream, "%s #%hhu\n", lstf_vm_opcode_to_string(opcode), upvalue_id))
                    goto err_write;
            }    break;

            case lstf_vm_op_vmcall:
            {
                uint8_t vmcode;
                if (!lstf_vm_program_read_imm_u8(prog, &offset, &vmcode))
                    goto err_read;
                if (!lstf_vm_vmcallcode_can_cast(vmcode)) {
                    if (!outputstream_printf(ostream, "<invalid VM call code %#hhx>\n", vmcode))
                        goto err_write;
                } else {
                    if (!outputstream_printf(ostream, "vmcall %s\n", lstf_vm_vmcallcode_to_string(vmcode)))
                        goto err_write;
                }
            }   break;

            case lstf_vm_op_jump:
            case lstf_vm_op_else:
            {
                uint64_t code_offset;

                if (!lstf_vm_program_read_imm_u64(prog, &offset, &code_offset))
                    goto err_read;
                if (!outputstream_printf(ostream, "%s <%#"PRIx64">\n", lstf_vm_opcode_to_string(opcode), code_offset))
                    goto err_write;
            }   break;

            case lstf_vm_op_exit:
            {
                int8_t retcode;

                if (!lstf_vm_program_read_imm_i8(prog, &offset, &retcode))
                    goto err_read;
                if (!outputstream_printf(ostream, "exit %hhu\n", retcode))
                    goto err_write;
            }   break;
            }
        }
    }
    return true;

err_read:
    outputstream_printf(ostream, "<could not read>\n");
    errno = EIO;
    return false;

err_write:
    return false;
}
