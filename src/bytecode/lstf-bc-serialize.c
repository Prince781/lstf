#include "lstf-bc-serialize.h"
#include "lstf-bc-program.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "io/outputstream.h"
#include "vm/lstf-vm-loader.h"
#include "json/json.h"
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>

static uint64_t
lstf_bc_program_get_instruction_offset(lstf_bc_program     *program,
                                       lstf_bc_function    *function,
                                       lstf_bc_instruction *instruction)
{
    ptr_hashmap_entry *entry = ptr_hashmap_get(program->code_offsets, function);
    uint64_t *offsets = entry->value;

    return offsets[instruction - function->instructions];
}

static uint64_t
lstf_bc_program_compute_debuginfo_size(lstf_bc_program *program)
{
    uint64_t current_offset = 0;

    if (!program->source_filename)
        return current_offset;

    current_offset += strlen(program->source_filename) + 1;
    current_offset += sizeof(uint64_t);
    for (iterator it = ptr_hashmap_iterator_create(program->debug_sourcemap); it.has_next; it = iterator_next(it)) {
        const ptr_hashmap_entry *entry = iterator_get_item(it);
        ptr_hashmap *function_sourcemap = entry->value;

        for (iterator it2 = ptr_hashmap_iterator_create(function_sourcemap); it2.has_next; it2 = iterator_next(it2)) {
            lstf_bc_debugentry *debug_entry = ((ptr_hashmap_entry *)iterator_get_item(it2))->value;

            current_offset += sizeof(uint64_t);
            current_offset += sizeof debug_entry->line;
            current_offset += sizeof debug_entry->column;
        }
    }

    current_offset += sizeof(uint64_t);
    for (iterator it = ptr_hashmap_iterator_create(program->debug_symbols); it.has_next; it = iterator_next(it)) {
        const ptr_hashmap_entry *entry = iterator_get_item(it);
        ptr_hashmap *function_symbols = entry->value;

        for (iterator it2 = ptr_hashmap_iterator_create(function_symbols); it2.has_next; it2 = iterator_next(it2)) {
            char *symbol_name = ((ptr_hashmap_entry *)iterator_get_item(it2))->value;

            current_offset += sizeof(uint64_t);
            current_offset += strlen(symbol_name) + 1;
        }
    }

    return current_offset;
}

static uint64_t
lstf_bc_program_compute_comments_size(lstf_bc_program *program)
{
    uint64_t current_offset = 0;

    for (iterator it = ptr_hashmap_iterator_create(program->comments); it.has_next; it = iterator_next(it)) {
        const ptr_hashmap_entry *entry = iterator_get_item(it);
        ptr_hashmap *function_comments = entry->value;

        for (iterator it2 = ptr_hashmap_iterator_create(function_comments); it2.has_next; it2 = iterator_next(it2)) {
            char *comment = ((ptr_hashmap_entry *)iterator_get_item(it2))->value;

            current_offset += sizeof(uint64_t);
            current_offset += strlen(comment) + 1;
        }
    }

    return current_offset;
}

static uint64_t
lstf_bc_program_compute_code_size_and_offsets(lstf_bc_program *program)
{
    uint64_t current_offset = 0;

    for (iterator it = ptr_hashmap_iterator_create(program->functions); it.has_next; it = iterator_next(it)) {
        ptr_hashmap_entry *entry = iterator_get_item(it);
        lstf_bc_function *function = entry->value;

        uint64_t *offsets = calloc(function->instructions_length, sizeof *offsets);
        if (!offsets) {
            fprintf(stderr, "%s: failed to allocate new offsets array: %s\n", __func__, strerror(errno));
            abort();
        }

        for (size_t i = 0; i < function->instructions_length; i++) {
            offsets[i] = current_offset;
            current_offset += lstf_bc_instruction_compute_size(&function->instructions[i]);
        }

        ptr_hashmap_insert(program->code_offsets, function, offsets);
    }

    return current_offset;
}

static lstf_bc_function *
lstf_bc_program_get_function(lstf_bc_program *program, const char *name)
{
    ptr_hashmap_entry *entry = ptr_hashmap_get(program->functions, name);

    return entry ? entry->value : NULL;
}

bool lstf_bc_program_serialize_to_binary(lstf_bc_program *program, outputstream *ostream)
{
    // compute sizes and offsets for various sections
    const uint64_t debuginfo_size = lstf_bc_program_compute_debuginfo_size(program);
    const uint64_t comments_size = lstf_bc_program_compute_comments_size(program);
    const uint64_t data_size = program->data_length;
    const uint64_t code_size = lstf_bc_program_compute_code_size_and_offsets(program);

    lstf_bc_function *main_function = lstf_bc_program_get_function(program, "main");
    assert(main_function && main_function->instructions_length > 0 &&
            "serializing LSTFC binary without a main function!");
    const uint64_t entry_point_code_offset = 
        lstf_bc_program_get_instruction_offset(program, main_function, &main_function->instructions[0]);

    // program header
    if (!outputstream_write(ostream, LSTFC_MAGIC_HEADER, sizeof LSTFC_MAGIC_HEADER))
        return false;

    if (!outputstream_write_uint64(ostream, entry_point_code_offset))
        return false;

    if (debuginfo_size > 0) {
        if (!outputstream_write_string(ostream, "debuginfo") || !outputstream_write_byte(ostream, '\0'))
            return false;
        if (!outputstream_write_uint64(ostream, debuginfo_size))
            return false;
    }

    if (comments_size > 0) {
        if (!outputstream_write_string(ostream, "comments") || !outputstream_write_byte(ostream, '\0'))
            return false;
        if (!outputstream_write_uint64(ostream, comments_size))
            return false;
    }

    if (data_size > 0) {
        if (!outputstream_write_string(ostream, "data") || !outputstream_write_byte(ostream, '\0'))
            return false;
        if (!outputstream_write_uint64(ostream, data_size))
            return false;
    }

    if (!outputstream_write_string(ostream, "code") || !outputstream_write_byte(ostream, '\0'))
        return false;
    if (!outputstream_write_uint64(ostream, code_size))
        return false;
    if (!outputstream_write_byte(ostream, '\0'))
        return false;

    // debuginfo section
    if (debuginfo_size > 0) {
        size_t filename_sz = 0;
        if (!(filename_sz = outputstream_write_string(ostream, program->source_filename)) ||
                !(filename_sz = outputstream_write_byte(ostream, '\0')))
            return false;
        // pad to N-byte boundary (alignment of lstf_bc_debugentry)
        if (filename_sz % alignof(lstf_bc_debugentry) > 0)
            for (filename_sz = alignof(lstf_bc_debugentry) - filename_sz % alignof(lstf_bc_debugentry);
                    filename_sz > 0; filename_sz--)
                if (!outputstream_write_byte(ostream, '\0'))
                return false;
        // n_debug_entries
        uint64_t n_debug_entries = 0;
        for (iterator it = ptr_hashmap_iterator_create(program->debug_sourcemap); it.has_next; it = iterator_next(it))
            n_debug_entries += ptr_hashmap_num_elements(((ptr_hashmap_entry *)iterator_get_item(it))->value);

        if (!outputstream_write_uint64(ostream, n_debug_entries))
            return false;
        // debug_entries
        for (iterator it = ptr_hashmap_iterator_create(program->debug_sourcemap); it.has_next; it = iterator_next(it)) {
            const ptr_hashmap_entry *entry = iterator_get_item(it);
            lstf_bc_function *function = entry->key;
            ptr_hashmap *function_sourcemap = entry->value;
            for (iterator it2 = ptr_hashmap_iterator_create(function_sourcemap); it2.has_next; it2 = iterator_next(it2)) {
                const ptr_hashmap_entry *entry2 = iterator_get_item(it2);
                lstf_bc_instruction *instruction = entry2->key;
                lstf_bc_debugentry *debug_entry = entry2->value;
                uint64_t instruction_offset = lstf_bc_program_get_instruction_offset(program, function, instruction);

                if (!outputstream_write_uint64(ostream, instruction_offset))
                    return false;

                if (!outputstream_write_uint32(ostream, debug_entry->line))
                    return false;

                if (!outputstream_write_uint32(ostream, debug_entry->column))
                    return false;
            }
        }

        uint64_t n_debug_symbols = 0;
        for (iterator it = ptr_hashmap_iterator_create(program->debug_symbols); it.has_next; it = iterator_next(it))
            n_debug_symbols += ptr_hashmap_num_elements(((ptr_hashmap_entry *)iterator_get_item(it))->value);

        if (!outputstream_write_uint64(ostream, n_debug_entries))
            return false;
        // debug symbols
        for (iterator it = ptr_hashmap_iterator_create(program->debug_symbols); it.has_next; it = iterator_next(it)) {
            const ptr_hashmap_entry *entry = iterator_get_item(it);
            lstf_bc_function *function = entry->key;
            ptr_hashmap *function_symbols = entry->value;
            for (iterator it2 = ptr_hashmap_iterator_create(function_symbols); it2.has_next; it2 = iterator_next(it2)) {
                const ptr_hashmap_entry *entry2 = iterator_get_item(it2);
                lstf_bc_instruction *instruction = entry2->key;
                char *symbol_name = entry2->value;
                uint64_t instruction_offset = lstf_bc_program_get_instruction_offset(program, function, instruction);

                if (!outputstream_write_uint64(ostream, instruction_offset))
                    return false;

                size_t symbol_name_sz = 0;
                if (!(symbol_name_sz = outputstream_write_string(ostream, symbol_name)) ||
                        !(symbol_name_sz = outputstream_write_byte(ostream, '\0')))
                    return false;

                // pad the symbol name to `lstf_vm_debugsym` boundary
                if (symbol_name_sz % alignof(lstf_vm_debugsym) > 0)
                    for (symbol_name_sz = alignof(lstf_vm_debugsym) - symbol_name_sz % alignof(lstf_vm_debugsym);
                            symbol_name_sz > 0; symbol_name_sz--)
                        if (!outputstream_write_byte(ostream, '\0'))
                            return false;
            }
        }
    }
    
    // comments section
    if (comments_size > 0) {
        for (iterator it = ptr_hashmap_iterator_create(program->comments); it.has_next; it = iterator_next(it)) {
            const ptr_hashmap_entry *entry = iterator_get_item(it);
            lstf_bc_function *function = entry->key;
            ptr_hashmap *function_comments = entry->value;
            for (iterator it2 = ptr_hashmap_iterator_create(function_comments); it2.has_next; it2 = iterator_next(it2)) {
                const ptr_hashmap_entry *entry2 = iterator_get_item(it2);
                lstf_bc_instruction *instruction = entry2->key;
                char *comment = entry2->value;
                uint64_t instruction_offset = lstf_bc_program_get_instruction_offset(program, function, instruction);

                if (!outputstream_write_uint64(ostream, instruction_offset))
                    return false;

                if (!outputstream_write_string(ostream, comment) || !outputstream_write_byte(ostream, '\0'))
                    return false;
            }
        }
    }

    // data section
    if (data_size > 0) {
        if (!outputstream_write(ostream, program->data, program->data_length))
            return false;
    }

    // code section
    for (iterator it = ptr_hashmap_iterator_create(program->functions); it.has_next; it = iterator_next(it)) {
        lstf_bc_function *function = ((ptr_hashmap_entry *)iterator_get_item(it))->value;

        for (size_t i = 0; i < function->instructions_length; i++) {
            lstf_bc_instruction *instruction = &function->instructions[i];
            char *json_expression_string = NULL;

            if (!outputstream_write_byte(ostream, instruction->opcode))
                return false;

            switch (instruction->opcode) {
            case lstf_vm_op_load_frameoffset:
                if (!outputstream_write_int64(ostream, instruction->frame_offset))
                    return false;
                break;
            case lstf_vm_op_load_dataoffset:
                if (!outputstream_write_uint64(ostream, instruction->data_offset - program->data))
                    return false;
                break;
            case lstf_vm_op_load_codeoffset:
                if (!outputstream_write_uint64(ostream, 
                            lstf_bc_program_get_instruction_offset(program,
                                                                   instruction->function_ref,
                                                                   &instruction->function_ref->instructions[0])))
                    return false;
                break;
            case lstf_vm_op_load_expression:
                json_expression_string = json_node_to_string(instruction->json_expression, false);
                if (!outputstream_write_string(ostream, json_expression_string) ||
                        !outputstream_write_byte(ostream, '\0')) {
                    free(json_expression_string);
                    return false;
                }
                free(json_expression_string);
                break;
            case lstf_vm_op_store:
                if (!outputstream_write_int64(ostream, instruction->frame_offset))
                    return false;
                break;
            case lstf_vm_op_pop:
            case lstf_vm_op_get:
            case lstf_vm_op_set:
            case lstf_vm_op_in:
                break;
            case lstf_vm_op_params:
                if (!outputstream_write_byte(ostream, instruction->num_parameters))
                    return false;
                break;
            case lstf_vm_op_call:
            case lstf_vm_op_schedule:
                if (!outputstream_write_uint64(ostream,
                            lstf_bc_program_get_instruction_offset(program,
                                                                   instruction->function_ref,
                                                                   &instruction->function_ref->instructions[0])))
                    return false;
                break;
            case lstf_vm_op_calli:
            case lstf_vm_op_schedulei:
            case lstf_vm_op_return:
                break;
            case lstf_vm_op_closure:
                if (!outputstream_write_byte(ostream, instruction->closure.num_upvalues))
                    return false;
                if (!outputstream_write_uint64(ostream,
                            lstf_bc_program_get_instruction_offset(program,
                                                                   instruction->closure.function_ref,
                                                                   &instruction->closure.function_ref->instructions[0])))
                    return false;
                for (unsigned up_i = 0; up_i < instruction->closure.num_upvalues; up_i++) {
                    if (!outputstream_write_byte(ostream, instruction->closure.upvalues[up_i].is_local))
                        return false;
                    if (!outputstream_write_int64(ostream, instruction->closure.upvalues[up_i].index))
                        return false;
                }
                break;
            case lstf_vm_op_upget:
                if (!outputstream_write_byte(ostream, instruction->upvalue_id))
                    return false;
                break;
            case lstf_vm_op_upset:
                if (!outputstream_write_byte(ostream, instruction->upvalue_id))
                    return false;
                break;
            case lstf_vm_op_vmcall:
                if (!outputstream_write_byte(ostream, instruction->vmcall_code))
                    return false;
                break;
            case lstf_vm_op_else:
            case lstf_vm_op_jump:
                assert(instruction->instruction_ref && "cannot serialize unresolved jump instruction!");
                if (!outputstream_write_uint64(ostream,
                            lstf_bc_program_get_instruction_offset(program,
                                                                   function,
                                                                   instruction->instruction_ref)))
                    return false;
                break;
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
            case lstf_vm_op_assert:
                break;
            case lstf_vm_op_exit:
                if (!outputstream_write_byte(ostream, instruction->exit_code))
                    return false;
                break;
            }
        }
    }

    return true;
}
