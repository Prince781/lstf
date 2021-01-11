#include "lstf-virtualmachine.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/string-builder.h"
#include "lstf-vm-opcodes.h"
#include "lstf-vm-program.h"
#include "lstf-vm-stack.h"
#include "lstf-vm-status.h"
#include "lstf-vm-value.h"
#include "util.h"
#include "json/json.h"
#include "json/json-parser.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

lstf_virtualmachine *
lstf_virtualmachine_new(lstf_vm_program *program, bool debug)
{
    lstf_virtualmachine *vm = calloc(1, sizeof *vm);

    vm->stack = lstf_vm_stack_new();
    vm->program = lstf_vm_program_ref(program);
    vm->pc = program->entry_point;
    vm->breakpoints = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, NULL, NULL);
    vm->debug = debug;

    return vm;
}

void lstf_virtualmachine_destroy(lstf_virtualmachine *vm)
{
    lstf_vm_program_unref(vm->program);
    ptr_hashmap_destroy(vm->breakpoints);
    lstf_vm_stack_destroy(vm->stack);
    free(vm);
}

static lstf_vm_status
lstf_virtualmachine_read_byte(lstf_virtualmachine *vm,
                              uint8_t             *byte)
{
    if (vm->pc < vm->program->code || vm->pc >= vm->program->code + vm->program->code_size)
        return lstf_vm_status_invalid_code_offset;

    *byte = *vm->pc++;
    return lstf_vm_status_continue;
}

static lstf_vm_status
lstf_virtualmachine_read_signed_byte(lstf_virtualmachine *vm,
                                     int8_t              *byte)
{
    if (vm->pc < vm->program->code || vm->pc >= vm->program->code + vm->program->code_size)
        return lstf_vm_status_invalid_instruction;

    *byte = *vm->pc++;
    return lstf_vm_status_continue;
}

static lstf_vm_status
lstf_virtualmachine_read_integer(lstf_virtualmachine *vm,
                                 uint64_t            *integer)
{
    uint64_t value = 0;
    lstf_vm_status status = lstf_vm_status_continue;

    // we read in an integer in most-significant byte first (big endian / network byte order)
    for (unsigned i = 0; i < sizeof(value); i++) {
        uint8_t byte;
        if ((status = lstf_virtualmachine_read_byte(vm, &byte)))
            return status;
        value |= byte << ((sizeof(value) - 1 - i) * CHAR_BIT);
    }

    *integer = value;

    return status;
}

static lstf_vm_status
lstf_virtualmachine_read_signed_integer(lstf_virtualmachine *vm,
                                        int64_t             *integer)
{
    uint64_t value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_virtualmachine_read_integer(vm, &value)))
        return status;

    *integer = (int64_t)value;
    return status;
}

static lstf_vm_status
lstf_virtualmachine_read_string(lstf_virtualmachine *vm,
                                char               **expression_string)
{
    lstf_vm_status status = lstf_vm_status_continue;
    char *string_beginning = (char *)vm->pc;
    uint8_t byte;

    while ((status = lstf_virtualmachine_read_byte(vm, &byte)) == lstf_vm_status_continue) {
        if (byte == '\0')
            break;
    }

    if (status == lstf_vm_status_continue)
        *expression_string = string_beginning;

    return status;
}

static lstf_vm_status
lstf_vm_op_jump_exec(lstf_virtualmachine *vm);

static lstf_vm_status
lstf_vm_op_load_frameoffset_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    int64_t fp_offset;
    lstf_vm_value value;

    if ((status = lstf_virtualmachine_read_signed_integer(vm, &fp_offset)))
        return status;

    if ((status = lstf_vm_stack_get_value(vm->stack, fp_offset, &value)))
        return status;

    if ((status = lstf_vm_stack_push_value(vm->stack, &value)))
        lstf_vm_value_clear(&value);

    return status;
}

static lstf_vm_status
lstf_vm_op_load_dataoffset_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint64_t data_offset;
    char *expression_string = NULL;
    json_node *node = NULL;
    lstf_vm_value value;

    if ((status = lstf_virtualmachine_read_integer(vm, &data_offset)))
        return status;

    if (data_offset >= vm->program->data_size)
        return lstf_vm_status_invalid_data_offset;

    expression_string = (char *)vm->program->data + data_offset;

    if (!(node = json_parser_parse_string(expression_string)))
        return lstf_vm_status_invalid_expression;

    value = lstf_vm_value_from_json_node(node);
    if ((status = lstf_vm_stack_push_value(vm->stack, &value)))
        lstf_vm_value_clear(&value);

    return status;
}

static lstf_vm_status
lstf_vm_op_load_codeoffset_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint64_t code_offset;

    if ((status = lstf_virtualmachine_read_integer(vm, &code_offset)))
        return status;

    status = lstf_vm_stack_push_code_address(vm->stack, vm->program->code + code_offset);

    return status;
}

static lstf_vm_status
lstf_vm_op_load_expression_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    // the expression string is part of the code section, and doesn't need to
    // be free()'d
    char *expression_string = NULL;
    json_node *node = NULL;
    lstf_vm_value value;

    if ((status = lstf_virtualmachine_read_string(vm, &expression_string)))
        return status;

    // parse JSON expression
    if (!(node = json_parser_parse_string(expression_string)))
        return lstf_vm_status_invalid_expression;

    value = lstf_vm_value_from_json_node(node);
    if ((status = lstf_vm_stack_push_value(vm->stack, &value)))
        lstf_vm_value_clear(&value);

    return status;
}

static lstf_vm_status
lstf_vm_op_store_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    int64_t fp_offset;
    lstf_vm_value value;

    // read the frame pointer offset (immediate)
    if ((status = lstf_virtualmachine_read_signed_integer(vm, &fp_offset)))
        return status;

    // pop the last value on the stack
    if ((status = lstf_vm_stack_pop_value(vm->stack, &value)))
        return status;

    if ((status = lstf_vm_stack_set_value(vm->stack, fp_offset, &value)))
        lstf_vm_value_clear(&value);

    return status;
}

static lstf_vm_status
lstf_vm_op_get_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    json_node *node;
    lstf_vm_value index;

    // pop index
    if ((status = lstf_vm_stack_pop_value(vm->stack, &index)))
        return status;

    if ((status = lstf_vm_stack_pop_object(vm->stack, &node)) == lstf_vm_status_continue) {
        if (index.value_type == lstf_vm_value_type_string) {
            json_node *member_value = NULL;
            if ((member_value = json_object_get_member(node, index.data.string->buffer))) {
                lstf_vm_value value = lstf_vm_value_from_json_node(member_value);
                if ((status = lstf_vm_stack_push_value(vm->stack, &value)))
                    lstf_vm_value_clear(&value);
            } else {
                // the JSON object's member does not exist
                status = lstf_vm_status_invalid_member_access;
            }
        } else {
            // we can only access a JSON object with a string
            status = lstf_vm_status_invalid_operand_type;
        }

        json_node_unref(node);
    } else if ((status = lstf_vm_stack_pop_array(vm->stack, &node)) == lstf_vm_status_continue) {
        if (index.value_type == lstf_vm_value_type_integer) {
            json_node *member_value = NULL;
            if ((member_value = json_array_get_element(node, index.data.integer))) {
                lstf_vm_value value = lstf_vm_value_from_json_node(member_value);
                if ((status = lstf_vm_stack_push_value(vm->stack, &value)))
                    lstf_vm_value_clear(&value);
            } else {
                // the JSON array's member does not exist
                status = lstf_vm_status_invalid_member_access;
            }
        } else {
            // we can only index into an array with an integer
            status = lstf_vm_status_invalid_operand_type;
        }
        json_node_unref(node);
    }

    lstf_vm_value_clear(&index);

    return status;
}

static lstf_vm_status
lstf_vm_op_set_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    json_node *node;
    lstf_vm_value index;
    lstf_vm_value value;

    // pop value
    if ((status = lstf_vm_stack_pop_value(vm->stack, &value)))
        return status;

    // pop index
    if ((status = lstf_vm_stack_pop_value(vm->stack, &index))) {
        lstf_vm_value_clear(&value);
        return status;
    }

    if ((status = lstf_vm_stack_pop_object(vm->stack, &node)) == lstf_vm_status_continue) {
        if (index.value_type == lstf_vm_value_type_string) {
            json_object_set_member(node, index.data.string->buffer, lstf_vm_value_to_json_node(value));
        } else {
            status = lstf_vm_status_invalid_operand_type;
        }
        json_node_unref(node);
    } else if ((status = lstf_vm_stack_pop_array(vm->stack, &node)) == lstf_vm_status_continue) {
        if (index.value_type == lstf_vm_value_type_integer) {
            unsigned array_index = (unsigned) index.data.integer;
            if (array_index < ((json_array *)node)->num_elements) {
                json_array_set_element(node, array_index, lstf_vm_value_to_json_node(value));
            } else {
                status = lstf_vm_status_index_out_of_bounds;
            }
        } else {
            status = lstf_vm_status_invalid_operand_type;
        }
        json_node_unref(node);
    }

    lstf_vm_value_clear(&value);
    lstf_vm_value_clear(&index);

    return status;
}

static lstf_vm_status
lstf_vm_op_call_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint64_t code_offset;

    // read code offset
    if ((status = lstf_virtualmachine_read_integer(vm, &code_offset)))
        return status;

    // set up a new stack frame
    if ((status = lstf_vm_stack_setup_frame(vm->stack)))
        return status;

    // save return address
    if ((status = lstf_vm_stack_push_code_address(vm->stack, vm->pc)))
        return status;

    // jump to new address
    vm->pc = vm->program->code + code_offset;

    return status;
}

static lstf_vm_status
lstf_vm_op_indirect_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    int64_t code_offset;

    if ((status = lstf_vm_stack_pop_integer(vm->stack, &code_offset)))
        return status;

    // set up a new stack frame
    if ((status = lstf_vm_stack_setup_frame(vm->stack)))
        return status;

    // save return address
    if ((status = lstf_vm_stack_push_code_address(vm->stack, vm->pc)))
        return status;

    // jump to new address
    vm->pc = vm->program->code + code_offset;
    
    return status;
}

static lstf_vm_status
lstf_vm_op_return_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint8_t *return_address;

    if ((status = lstf_vm_stack_pop_code_address(vm->stack, &return_address)))
        return status;

    if ((status = lstf_vm_stack_teardown_frame(vm->stack)))
        return status;

    vm->pc = return_address;

    return status;
}

static lstf_vm_status (*const virtualmachine_calls[256])(lstf_virtualmachine *) = {
    [lstf_vm_vmcall_connect]        = NULL /* TODO */,
    [lstf_vm_vmcall_td_open]        = NULL /* TODO */,
    [lstf_vm_vmcall_diagnostics]    = NULL /* TODO */,
};

static lstf_vm_status
lstf_vm_op_vmcall_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint8_t vmcall_code;

    if ((status = lstf_virtualmachine_read_byte(vm, &vmcall_code)))
        return status;

    if (!virtualmachine_calls[vmcall_code])
        status = lstf_vm_status_invalid_vmcall;
    else
        status = virtualmachine_calls[vmcall_code](vm);

    return status;
}

static lstf_vm_status
lstf_vm_op_if_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    bool expression_result;

    if ((status = lstf_vm_stack_pop_boolean(vm->stack, &expression_result)))
        return status;

    // early exit, and continue to the next instruction if the expression
    // result did not evaluate to `true`
    if (!expression_result)
        return status;

    // otherwise, perform a jump
    if ((status = lstf_vm_op_jump_exec(vm)))
        return status;

    return status;
}

static lstf_vm_status
lstf_vm_op_jump_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint64_t code_offset;

    // read code offset
    if ((status = lstf_virtualmachine_read_integer(vm, &code_offset)))
        return status;

    // verify code offset
    uint8_t *new_pc = vm->pc + code_offset;
    if (new_pc < vm->program->code || new_pc >= vm->program->code + vm->program->code_size)
        return lstf_vm_status_invalid_code_offset;

    // save current PC
    if ((status = lstf_vm_stack_push_code_address(vm->stack, vm->pc)))
        return status;

    // jump to code offset
    vm->pc = new_pc;

    return status;
}

static lstf_vm_status
lstf_vm_op_bool_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    lstf_vm_value value;

    if ((status = lstf_vm_stack_pop_value(vm->stack, &value)))
        return status;

    switch (value.value_type) {
    case lstf_vm_value_type_array_ref:
    case lstf_vm_value_type_object_ref:
    case lstf_vm_value_type_pattern_ref:
        if ((status = lstf_vm_stack_push_boolean(vm->stack, true)))
            return status;
        json_node_unref(value.data.json_node_ref);
        break;
    case lstf_vm_value_type_boolean:
        if ((status = lstf_vm_stack_push_boolean(vm->stack, value.data.boolean)))
            return status;
        break;
    case lstf_vm_value_type_code_address:
        if ((status = lstf_vm_stack_push_boolean(vm->stack, value.data.address)))
            return status;
        break;
    case lstf_vm_value_type_double:
        if ((status = lstf_vm_stack_push_boolean(vm->stack, value.data.double_value)))
            return status;
        break;
    case lstf_vm_value_type_integer:
        if ((status = lstf_vm_stack_push_boolean(vm->stack, value.data.integer)))
            return status;
        break;
    case lstf_vm_value_type_null:
        if ((status = lstf_vm_stack_push_boolean(vm->stack, false)))
            return status;
        break;
    case lstf_vm_value_type_string:
        if ((status = lstf_vm_stack_push_boolean(vm->stack, !string_is_empty(value.data.string))))
            return status;
        break;
    }

    return status;
}

static lstf_vm_status
lstf_vm_op_land_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    bool operand1, operand2;

    if ((status = lstf_vm_stack_pop_boolean(vm->stack, &operand2)))
        return status;

    if ((status = lstf_vm_stack_pop_boolean(vm->stack, &operand1)))
        return status;

    if ((status = lstf_vm_stack_push_boolean(vm->stack, operand1 && operand2)))
        return status;

    return status;
}

static lstf_vm_status
lstf_vm_op_lor_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    bool operand1, operand2;

    if ((status = lstf_vm_stack_pop_boolean(vm->stack, &operand2)))
        return status;

    if ((status = lstf_vm_stack_pop_boolean(vm->stack, &operand1)))
        return status;

    if ((status = lstf_vm_stack_push_boolean(vm->stack, operand1 || operand2)))
        return status;

    return status;
}

static lstf_vm_status
lstf_vm_op_lnot_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    bool operand;

    if ((status = lstf_vm_stack_pop_boolean(vm->stack, &operand)))
        return status;

    if ((status = lstf_vm_stack_push_boolean(vm->stack, !operand)))
        return status;

    return status;
}

static lstf_vm_status
lstf_vm_op_print_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    lstf_vm_value value;

    if ((status = lstf_vm_stack_pop_value(vm->stack, &value)))
        return status;

    switch (value.value_type) {
        case lstf_vm_value_type_array_ref:
        case lstf_vm_value_type_object_ref:
        case lstf_vm_value_type_pattern_ref:
        {
            char *json_representation = json_node_represent_string(value.data.json_node_ref, true);
            printf("%s\n", json_representation);
            free(json_representation);
            json_node_unref(value.data.json_node_ref);      // cleanup value
        }   break;
        case lstf_vm_value_type_boolean:
            printf("%s\n", value.data.boolean ? "true" : "false");
            break;
        case lstf_vm_value_type_code_address:
            printf("0x%p\n", (void *)value.data.address);
            break;
        case lstf_vm_value_type_double:
            printf("%lf\n", value.data.double_value);
            break;
        case lstf_vm_value_type_integer:
            printf("%"PRIi64"\n", value.data.integer);
            break;
        case lstf_vm_value_type_null:
            printf("null\n");
            break;
        case lstf_vm_value_type_string:
            printf("%s\n", value.data.string->buffer);
            string_unref(value.data.string);                // cleanup
            break;
    }

    return status;
}

static lstf_vm_status
lstf_vm_op_exit_exec(lstf_virtualmachine *vm)
{
    lstf_vm_status status = lstf_vm_status_continue;
    int8_t return_code;

    if ((status = lstf_virtualmachine_read_signed_byte(vm, &return_code)))
        return status;

    vm->return_code = return_code;
    status = lstf_vm_status_exited;

    return status;
}

static lstf_vm_status (*const instruction_table[256])(lstf_virtualmachine *) = {
    // --- reading/writing to/from memory
    [lstf_vm_op_load_frameoffset]   = lstf_vm_op_load_frameoffset_exec,
    [lstf_vm_op_load_dataoffset]    = lstf_vm_op_load_dataoffset_exec,
    [lstf_vm_op_load_codeoffset]    = lstf_vm_op_load_codeoffset_exec,
    [lstf_vm_op_load_expression]    = lstf_vm_op_load_expression_exec,
    [lstf_vm_op_store]              = lstf_vm_op_store_exec,
    // --- accessing members of a structured type
    [lstf_vm_op_get]                = lstf_vm_op_get_exec,
    [lstf_vm_op_set]                = lstf_vm_op_set_exec,
    // --- functions
    [lstf_vm_op_call]               = lstf_vm_op_call_exec,
    [lstf_vm_op_indirect]           = lstf_vm_op_indirect_exec,
    [lstf_vm_op_return]             = lstf_vm_op_return_exec,
    [lstf_vm_op_vmcall]             = lstf_vm_op_vmcall_exec,
    // --- control flow
    [lstf_vm_op_if]                 = lstf_vm_op_if_exec,
    [lstf_vm_op_jump]               = lstf_vm_op_jump_exec,
    // --- logical operations
    [lstf_vm_op_bool]               = lstf_vm_op_bool_exec,
    [lstf_vm_op_land]               = lstf_vm_op_land_exec,
    [lstf_vm_op_lor]                = lstf_vm_op_lor_exec,
    [lstf_vm_op_lnot]               = lstf_vm_op_lnot_exec,
    // --- input/output
    [lstf_vm_op_print]              = lstf_vm_op_print_exec,
    [lstf_vm_op_exit]               = lstf_vm_op_exit_exec
};

bool
lstf_virtualmachine_run(lstf_virtualmachine *vm)
{
    while (true) {
        if (vm->should_stop)
            return true;

        if (vm->last_status != lstf_vm_status_continue)
            return false;

        if (vm->debug) {
            // TODO: breakpoints, watchpoints
        }

        if (vm->pc == vm->program->entry_point) {
            // setup initial stack frame
            if ((vm->last_status = lstf_vm_stack_setup_frame(vm->stack)))
                return false;
        }

        uint8_t opcode;
        if ((vm->last_status = lstf_virtualmachine_read_byte(vm, &opcode)))
            return false;

        if (!instruction_table[opcode]) {
            vm->last_status = lstf_vm_status_invalid_instruction;
        } else {
            vm->last_status = instruction_table[opcode](vm);
        }
    }
}
