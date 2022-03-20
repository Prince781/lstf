#include "lstf-virtualmachine.h"
#include "data-structures/ptr-hashset.h"
#include "data-structures/ptr-list.h"
#include "data-structures/string-builder.h"
#include "io/event.h"
#include "io/outputstream.h"
#include "lstf-common.h"
#include "lstf-vm-opcodes.h"
#include "lstf-vm-program.h"
#include "lstf-vm-stack.h"
#include "lstf-vm-status.h"
#include "lstf-vm-value.h"
#include "lstf-vm-coroutine.h"
#include "util.h"
#include "json/json.h"
#include "json/json-parser.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * The number of dynamic instructions before a context switch to another
 * running coroutine should happen.
 *
 * Smaller values should make coroutines more responsive when you have many
 * executing simulatenously but will cause more context switches and might
 * degrade instruction throughput.
 */
#define LSTF_VM_CONTEXT_SWITCH_CYCLES 64

lstf_virtualmachine *
lstf_virtualmachine_new(lstf_vm_program *program,
                        outputstream    *ostream,
                        bool             debug)
{
    assert(program && "cannot create VM without a program!");
    lstf_virtualmachine *vm = calloc(1, sizeof *vm);

    if (!vm) {
        perror("failed to create lstf_virtualmachine");
        abort();
    }

    vm->program = lstf_vm_program_ref(program);
    if (!ostream)
        ostream = outputstream_new_from_file(stdout, false);
    vm->ostream = outputstream_ref(ostream);
    vm->run_queue = ptr_list_new((collection_item_ref_func) lstf_vm_coroutine_ref,
            (collection_item_unref_func) lstf_vm_coroutine_unref);
    vm->suspended_list = ptr_list_new((collection_item_ref_func) lstf_vm_coroutine_ref,
            (collection_item_unref_func) lstf_vm_coroutine_unref);
    vm->event_loop = eventloop_new();
    vm->breakpoints = ptr_hashset_new(ptrhash, NULL, NULL, NULL);
    vm->debug = debug;

    return vm;
}

void lstf_virtualmachine_destroy(lstf_virtualmachine *vm)
{
    lstf_vm_program_unref(vm->program);
    outputstream_unref(vm->ostream);
    ptr_list_destroy(vm->run_queue);
    ptr_list_destroy(vm->suspended_list);
    lstf_vm_coroutine_unref(vm->main_coroutine);
    eventloop_destroy(vm->event_loop);
    ptr_hashset_destroy(vm->breakpoints);
    free(vm);
}

static lstf_vm_status
lstf_virtualmachine_read_byte(lstf_virtualmachine *vm,
                              lstf_vm_coroutine   *cr,
                              uint8_t             *byte)
{
    if (!cr->pc || cr->pc == vm->program->code + vm->program->code_size)
        return lstf_vm_status_invalid_code_offset;

    *byte = *cr->pc++;
    return lstf_vm_status_continue;
}

static lstf_vm_status
lstf_virtualmachine_read_signed_byte(lstf_virtualmachine *vm,
                                     lstf_vm_coroutine   *cr,
                                     int8_t              *byte)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint8_t value = 0;

    if ((status = lstf_virtualmachine_read_byte(vm, cr, &value)))
        return status;

    *byte = value;
    return status;
}

static lstf_vm_status
lstf_virtualmachine_read_boolean(lstf_virtualmachine *vm,
                                 lstf_vm_coroutine   *cr,
                                 bool                *boolean)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint8_t byte = 0;

    if ((status = lstf_virtualmachine_read_byte(vm, cr, &byte)))
        return status;

    *boolean = byte != 0;
    return status;
}

static lstf_vm_status
lstf_virtualmachine_read_integer(lstf_virtualmachine *vm,
                                 lstf_vm_coroutine   *cr,
                                 uint64_t            *integer)
{
    uint64_t value = 0;
    lstf_vm_status status = lstf_vm_status_continue;

    // we read in an integer in most-significant byte first (big endian / network byte order)
    for (unsigned i = 0; i < sizeof(value); i++) {
        uint8_t byte;
        if ((status = lstf_virtualmachine_read_byte(vm, cr, &byte)))
            return status;
        value |= ((uint64_t)byte) << ((sizeof(value) - 1 - i) * CHAR_BIT);
    }

    if (integer)
        *integer = value;

    return status;
}

static lstf_vm_status
lstf_virtualmachine_read_signed_integer(lstf_virtualmachine *vm,
                                        lstf_vm_coroutine   *cr,
                                        int64_t             *integer)
{
    uint64_t value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_virtualmachine_read_integer(vm, cr, &value)))
        return status;

    if (integer)
        *integer = (int64_t)value;
    return status;
}

static lstf_vm_status
lstf_virtualmachine_read_string(lstf_virtualmachine *vm,
                                lstf_vm_coroutine   *cr,
                                char               **expression_string)
{
    lstf_vm_status status = lstf_vm_status_continue;
    char *string_beginning = (char *)cr->pc;
    uint8_t byte;

    while ((status = lstf_virtualmachine_read_byte(vm, cr, &byte)) == lstf_vm_status_continue) {
        if (byte == '\0')
            break;
    }

    if (expression_string && status == lstf_vm_status_continue)
        *expression_string = string_beginning;

    return status;
}

static lstf_vm_status
lstf_vm_op_jump_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr);

static lstf_vm_status
lstf_vm_op_load_frameoffset_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    int64_t fp_offset;
    lstf_vm_value value;

    if ((status = lstf_virtualmachine_read_signed_integer(vm, cr, &fp_offset)))
        return status;

    if ((status = lstf_vm_stack_frame_get_value(cr->stack, fp_offset, &value)))
        return status;

    if ((status = lstf_vm_stack_push_value(cr->stack, &value)))
        lstf_vm_value_clear(&value);

    return status;
}

static lstf_vm_status
lstf_vm_op_load_dataoffset_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint64_t data_offset;
    char *expression_string = NULL;
    json_node *node = NULL;
    lstf_vm_value value;

    if ((status = lstf_virtualmachine_read_integer(vm, cr, &data_offset)))
        return status;

    if (data_offset >= vm->program->data_size)
        return lstf_vm_status_invalid_data_offset;

    expression_string = (char *)vm->program->data + data_offset;

    if (!(node = json_parser_parse_string(expression_string)))
        return lstf_vm_status_invalid_expression;

    value = lstf_vm_value_from_json_node(node);
    if ((status = lstf_vm_stack_push_value(cr->stack, &value)))
        lstf_vm_value_clear(&value);

    return status;
}

static lstf_vm_status
lstf_vm_op_load_codeoffset_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint64_t code_offset;

    if ((status = lstf_virtualmachine_read_integer(vm, cr, &code_offset)))
        return status;

    if (code_offset >= vm->program->code_size)
        return lstf_vm_status_invalid_code_offset;

    status = lstf_vm_stack_push_code_address(cr->stack, vm->program->code + code_offset);

    return status;
}

static lstf_vm_status
lstf_vm_op_load_expression_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    // the expression string is part of the code section, and doesn't need to
    // be free()'d
    char *expression_string = NULL;
    json_node *node = NULL;
    lstf_vm_value value;

    if ((status = lstf_virtualmachine_read_string(vm, cr, &expression_string)))
        return status;

    // parse JSON expression
    if (!(node = json_parser_parse_string(expression_string)))
        return lstf_vm_status_invalid_expression;

    value = lstf_vm_value_from_json_node(node);
    if ((status = lstf_vm_stack_push_value(cr->stack, &value)))
        lstf_vm_value_clear(&value);

    return status;
}

static lstf_vm_status
lstf_vm_op_store_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    int64_t fp_offset;
    lstf_vm_value value;

    // read the frame pointer offset (immediate)
    if ((status = lstf_virtualmachine_read_signed_integer(vm, cr, &fp_offset)))
        return status;

    // pop the last value on the stack
    if ((status = lstf_vm_stack_pop_value(cr->stack, &value)))
        return status;

    if ((status = lstf_vm_stack_set_frame_value(cr->stack, fp_offset, &value)))
        lstf_vm_value_clear(&value);

    return status;
}

static lstf_vm_status
lstf_vm_op_pop_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    (void) vm;
    lstf_vm_status status = lstf_vm_status_continue;
    lstf_vm_value value;

    if ((status = lstf_vm_stack_pop_value(cr->stack, &value)))
        return status;

    lstf_vm_value_clear(&value);

    return status;
}

static lstf_vm_status
lstf_vm_op_get_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    (void) vm;
    lstf_vm_status status = lstf_vm_status_continue;
    json_node *node;
    lstf_vm_value index;

    // pop index
    if ((status = lstf_vm_stack_pop_value(cr->stack, &index)))
        return status;

    if ((status = lstf_vm_stack_pop_object(cr->stack, &node)) == lstf_vm_status_continue) {
        if (index.value_type == lstf_vm_value_type_string) {
            json_node *member_value = NULL;
            if ((member_value = json_object_get_member(node, index.data.string->buffer))) {
                lstf_vm_value value = lstf_vm_value_from_json_node(member_value);
                if ((status = lstf_vm_stack_push_value(cr->stack, &value)))
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
    } else if ((status = lstf_vm_stack_pop_array(cr->stack, &node)) == lstf_vm_status_continue) {
        if (index.value_type == lstf_vm_value_type_integer) {
            json_node *member_value = NULL;
            if ((member_value = json_array_get_element(node, index.data.integer))) {
                lstf_vm_value value = lstf_vm_value_from_json_node(member_value);
                if ((status = lstf_vm_stack_push_value(cr->stack, &value)))
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
lstf_vm_op_set_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    (void) vm;
    lstf_vm_status status = lstf_vm_status_continue;
    json_node *node;
    lstf_vm_value index;
    lstf_vm_value value;

    // pop value
    if ((status = lstf_vm_stack_pop_value(cr->stack, &value)))
        return status;

    // pop index
    if ((status = lstf_vm_stack_pop_value(cr->stack, &index))) {
        lstf_vm_value_clear(&value);
        return status;
    }

    if ((status = lstf_vm_stack_pop_object(cr->stack, &node)) == lstf_vm_status_continue) {
        if (index.value_type == lstf_vm_value_type_string) {
            json_object_set_member(node, index.data.string->buffer, lstf_vm_value_to_json_node(value));
        } else {
            status = lstf_vm_status_invalid_operand_type;
        }
        json_node_unref(node);
    } else if ((status = lstf_vm_stack_pop_array(cr->stack, &node)) == lstf_vm_status_continue) {
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
lstf_vm_op_append_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    (void) vm;
    lstf_vm_status status = lstf_vm_status_continue;
    json_node *array;
    lstf_vm_value value;

    if ((status = lstf_vm_stack_pop_value(cr->stack, &value)))
        return status;

    if ((status = lstf_vm_stack_pop_array(cr->stack, &array))) {
        lstf_vm_value_clear(&value);
        return status;
    }

    json_array_add_element(array, lstf_vm_value_to_json_node(value));

    lstf_vm_value_clear(&value);
    json_node_unref(array);
    return status;
}

static lstf_vm_status
lstf_vm_op_params_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint8_t num_parameters;
    uint8_t *return_address;

    if ((status = lstf_virtualmachine_read_byte(vm, cr, &num_parameters)))
        return status;

    if ((status = lstf_vm_stack_get_frame_return_address(cr->stack, &return_address)))
        return status;

    // if the return address is NULL, this is a coroutine and the
    // parameters are already passed in the initial stack frame
    if (return_address) {
        // load the parameters from the previous stack frame
        for (uint8_t i = 0; i < num_parameters; i++) {
            int64_t fp_offset = -(num_parameters - i);
            lstf_vm_value parameter;

            if ((status = lstf_vm_stack_frame_get_value(cr->stack, fp_offset, &parameter)))
                return status;

            if ((status = lstf_vm_stack_push_value(cr->stack, &parameter)))
                return status;
        }
    }

    return lstf_vm_stack_frame_set_parameters(cr->stack, num_parameters);
}

static lstf_vm_status
lstf_vm_op_call_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint64_t code_offset;

    // read code offset immediate value
    if ((status = lstf_virtualmachine_read_integer(vm, cr, &code_offset)))
        return status;

    // set up a new stack frame
    // the saved return address is the current PC
    if ((status = lstf_vm_stack_setup_frame(cr->stack, cr->pc, NULL)))
        return status;

    // jump to new address
    cr->pc = vm->program->code + code_offset;

    return status;
}

static lstf_vm_status
lstf_vm_op_calli_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    (void) vm;
    lstf_vm_status status = lstf_vm_status_continue;
    uint8_t *code_address = NULL;
    lstf_vm_closure *closure = NULL;

    // try to get the code address saved by the last instruction
    if ((status = lstf_vm_stack_pop_code_address(cr->stack, &code_address))) {
        if (status != lstf_vm_status_invalid_operand_type)
            // the error was something else, so we cannot continue
            return status;

        // if it was an invalid type, then maybe it was a closure
        if ((status = lstf_vm_stack_pop_closure(cr->stack, &closure)))
            return status;
        code_address = closure->code_address;
    }

    // set up a new stack frame
    // the saved return address is the current PC
    if ((status = lstf_vm_stack_setup_frame(cr->stack, cr->pc, closure)))
        return status;

    // jump to new address
    cr->pc = code_address;

    if (closure)
        lstf_vm_closure_unref(closure);
    
    return status;
}

static lstf_vm_status
lstf_vm_schedule_new_coroutine(lstf_virtualmachine *vm,
                               lstf_vm_coroutine   *cr,
                               uint8_t              num_params,
                               uint8_t             *code_address,
                               lstf_vm_closure     *closure)
{
    lstf_vm_status status = lstf_vm_status_continue;
    // create a new coroutine
    lstf_vm_coroutine *new_cr = lstf_vm_coroutine_new(code_address);
    // set up initial stack frame for new coroutine
    // the saved return address is NULL, because when the coroutine returns it exits
    if ((status = lstf_vm_stack_setup_frame(new_cr->stack, NULL, closure)))
        goto cleanup_coroutine;
    // pass parameters to the new coroutine, loaded from the current stack frame
    lstf_vm_value parameters[1u << (CHAR_BIT * sizeof num_params)] = { 0 };
    for (uint8_t i = 0; i < num_params; i++)
        if ((status = lstf_vm_stack_pop_value(cr->stack, &parameters[num_params - i - 1])))
            goto cleanup_coroutine;
    for (uint8_t i = 0; i < num_params; i++)
        if ((status = lstf_vm_stack_push_value(new_cr->stack, &parameters[i])))
            goto cleanup_coroutine;
    // queue the coroutine for execution at a later point
    new_cr->node = ptr_list_append(vm->run_queue, new_cr);
    return status;

cleanup_coroutine:
    lstf_vm_coroutine_unref(new_cr);
    return status;
}

static lstf_vm_status
lstf_vm_op_schedule_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint64_t code_offset;
    uint8_t num_params;

    // read code offset immediate value
    if ((status = lstf_virtualmachine_read_integer(vm, cr, &code_offset)))
        return status;

    // read param count immediate value
    if ((status = lstf_virtualmachine_read_byte(vm, cr, &num_params)))
        return status;

    return lstf_vm_schedule_new_coroutine(vm, cr, num_params, vm->program->code + code_offset, NULL);
}

static lstf_vm_status
lstf_vm_op_schedulei_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint8_t *code_address = NULL;
    lstf_vm_closure *closure = NULL;
    uint8_t num_params;

    // try to get the code address saved by the last instruction
    if ((status = lstf_vm_stack_pop_code_address(cr->stack, &code_address))) {
        if (status != lstf_vm_status_invalid_operand_type)
            // the error was something else, so we cannot continue
            return status;

        // if it was an invalid type, then maybe it was a closure
        if ((status = lstf_vm_stack_pop_closure(cr->stack, &closure)))
            return status;
        code_address = closure->code_address;
    }

    // read param count immediate value
    if ((status = lstf_virtualmachine_read_byte(vm, cr, &num_params)))
        return status;

    return lstf_vm_schedule_new_coroutine(vm, cr, num_params, code_address, closure);
}

static lstf_vm_status
lstf_vm_op_return_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    (void) vm;
    lstf_vm_status status = lstf_vm_status_continue;
    uint8_t *return_address;

    if ((status = lstf_vm_stack_teardown_frame(cr->stack, &return_address)))
        return status;

    cr->pc = return_address;

    return status;
}

static lstf_vm_status
lstf_vm_op_closure_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint8_t num_upvalues = 0;
    lstf_vm_upvalue *upvalues[LSTF_VM_MAX_CAPTURES] = { 0 };
    int64_t fp_offsets[LSTF_VM_MAX_CAPTURES] = { 0 };
    uint64_t code_offset = 0;
    lstf_vm_closure *closure = NULL;

    if ((status = lstf_virtualmachine_read_byte(vm, cr, &num_upvalues)))
        return status;

    if ((status = lstf_virtualmachine_read_integer(vm, cr, &code_offset)))
        return status;

    if (code_offset >= vm->program->code_size)
        return lstf_vm_status_invalid_code_offset;

    for (uint8_t i = 0; i < num_upvalues; i++) {
        bool is_local = false;

        if ((status = lstf_virtualmachine_read_boolean(vm, cr, &is_local)))
            goto cleanup_upvalues;

        if (is_local) {
            // then [index] is the relative frame offset (fp_offset)
            int64_t fp_offset = 0;
            lstf_vm_value *value_ptr = NULL;

            if ((status = lstf_virtualmachine_read_signed_integer(vm, cr, &fp_offset)))
                goto cleanup_upvalues;

            if ((status = lstf_vm_stack_frame_get_value_address(cr->stack, fp_offset, &value_ptr)))
                goto cleanup_upvalues;

            // now check whether this stack offset is already captured in the current frame
            if ((status = lstf_vm_stack_frame_get_tracked_upvalue(cr->stack, fp_offset, &upvalues[i]))) {
                if (status != lstf_vm_status_invalid_upvalue)
                    goto cleanup_upvalues;
                upvalues[i] = lstf_vm_upvalue_new(value_ptr - cr->stack->values, cr);
                status = lstf_vm_status_continue;
            }
            fp_offsets[i] = fp_offset;
        } else {
            // then [index] is the n'th up-value belonging to the current closure
            uint64_t upvalue_id = 0;

            if ((status = lstf_virtualmachine_read_integer(vm, cr, &upvalue_id)))
                goto cleanup_upvalues;

            if (upvalue_id > LSTF_VM_MAX_CAPTURES) {
                status = lstf_vm_status_invalid_upvalue;
                goto cleanup_upvalues;
            }

            if ((status = lstf_vm_stack_frame_get_upvalue(cr->stack, upvalue_id, &upvalues[i])))
                goto cleanup_upvalues;

            fp_offsets[i] = -1;
        }

        upvalues[i] = lstf_vm_upvalue_ref(upvalues[i]);
    }

    closure = lstf_vm_closure_new(vm->program->code + code_offset, num_upvalues, upvalues);

    if ((status = lstf_vm_stack_push_closure(cr->stack, closure))) {
        lstf_vm_closure_unref(closure);
        goto cleanup_upvalues;
    }

    // finally, track the locals that are captured
    for (uint8_t i = 0; i < num_upvalues; i++)
        if (fp_offsets[i] != -1)
            lstf_vm_stack_frame_track_upvalue(cr->stack, fp_offsets[i], upvalues[i]);

cleanup_upvalues:
    // cleanup upvalues array
    for (uint8_t i = 0; i < num_upvalues; i++)
        if (upvalues[i])
            lstf_vm_upvalue_unref(upvalues[i]);
    return status;
}

static lstf_vm_status
lstf_vm_op_upget_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint8_t upvalue_id;
    lstf_vm_upvalue *upvalue;
    lstf_vm_value value;

    if ((status = lstf_virtualmachine_read_byte(vm, cr, &upvalue_id)))
        return status;

    if ((status = lstf_vm_stack_frame_get_upvalue(cr->stack, upvalue_id, &upvalue)))
        return status;

    if (upvalue->is_local) {
        if ((status = lstf_vm_stack_get_value(upvalue->cr->stack, upvalue->stack_offset, &value)))
            return status;
    } else {
        value = upvalue->value;
        // copy the value but don't take ownership of any underlying data
        value.takes_ownership = false;
    }

    if ((status = lstf_vm_stack_push_value(cr->stack, &value)))
        return status;

    return status;
}

static lstf_vm_status
lstf_vm_op_upset_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint8_t upvalue_id;
    lstf_vm_upvalue *upvalue;
    lstf_vm_value value;

    if ((status = lstf_virtualmachine_read_byte(vm, cr, &upvalue_id)))
        return status;

    if ((status = lstf_vm_stack_frame_get_upvalue(cr->stack, upvalue_id, &upvalue)))
        return status;

    if ((status = lstf_vm_stack_pop_value(cr->stack, &value)))
        return status;

    if (upvalue->is_local) {
        if ((status = lstf_vm_stack_set_value(upvalue->cr->stack, upvalue->stack_offset, &value)))
            return status;
    } else {
        lstf_vm_value_clear(&upvalue->value);
        upvalue->value = lstf_vm_value_take_ownership(&value);
    }

    return status;
}

static lstf_vm_status (*const virtualmachine_calls[256])(lstf_virtualmachine *, lstf_vm_coroutine *) = {
    [lstf_vm_vmcall_connect]        = NULL /* TODO */,
    [lstf_vm_vmcall_td_open]        = NULL /* TODO */,
    [lstf_vm_vmcall_diagnostics]    = NULL /* TODO */,
};

static lstf_vm_status
lstf_vm_op_vmcall_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint8_t vmcall_code;

    if ((status = lstf_virtualmachine_read_byte(vm, cr, &vmcall_code)))
        return status;

    if (!virtualmachine_calls[vmcall_code])
        status = lstf_vm_status_invalid_vmcall;
    else
        status = virtualmachine_calls[vmcall_code](vm, cr);

    return status;
}

static lstf_vm_status
lstf_vm_op_else_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    bool expression_result;

    if ((status = lstf_vm_stack_pop_boolean(cr->stack, &expression_result)))
        return status;

    // early exit, and continue to the next instruction if the expression
    // result evaluated to `true`
    if (expression_result)
        // discard the immediate value
        return lstf_virtualmachine_read_integer(vm, cr, NULL);

    // otherwise, perform a jump
    return lstf_vm_op_jump_exec(vm, cr);
}

static lstf_vm_status
lstf_vm_op_jump_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    uint64_t code_offset;

    // read code offset
    if ((status = lstf_virtualmachine_read_integer(vm, cr, &code_offset)))
        return status;

    // verify code offset
    if (code_offset > vm->program->code_size)
        return lstf_vm_status_invalid_code_offset;

    uint8_t *new_pc = vm->program->code + code_offset;

    // jump to code offset
    cr->pc = new_pc;

    return status;
}

static lstf_vm_status
lstf_vm_op_bool_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    (void) vm;
    lstf_vm_status status = lstf_vm_status_continue;
    lstf_vm_value value;

    if ((status = lstf_vm_stack_pop_value(cr->stack, &value)))
        return status;

    switch (value.value_type) {
    case lstf_vm_value_type_array_ref:
    case lstf_vm_value_type_object_ref:
    case lstf_vm_value_type_pattern_ref:
        status = lstf_vm_stack_push_boolean(cr->stack, true);
        break;
    case lstf_vm_value_type_boolean:
        status = lstf_vm_stack_push_boolean(cr->stack, value.data.boolean);
        break;
    case lstf_vm_value_type_code_address:
        status = lstf_vm_stack_push_boolean(cr->stack, value.data.address);
        break;
    case lstf_vm_value_type_double:
        status = lstf_vm_stack_push_boolean(cr->stack, value.data.double_value);
        break;
    case lstf_vm_value_type_integer:
        status = lstf_vm_stack_push_boolean(cr->stack, value.data.integer);
        break;
    case lstf_vm_value_type_null:
        status = lstf_vm_stack_push_boolean(cr->stack, false);
        break;
    case lstf_vm_value_type_string:
        status = lstf_vm_stack_push_boolean(cr->stack, !string_is_empty(value.data.string));
        break;
    case lstf_vm_value_type_closure:
        status = lstf_vm_stack_push_boolean(cr->stack, value.data.closure->code_address);
        break;
    }

    lstf_vm_value_clear(&value);
    return status;
}

static lstf_vm_status
lstf_vm_op_land_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    (void) vm;
    lstf_vm_status status = lstf_vm_status_continue;
    bool operand1, operand2;

    if ((status = lstf_vm_stack_pop_boolean(cr->stack, &operand2)))
        return status;

    if ((status = lstf_vm_stack_pop_boolean(cr->stack, &operand1)))
        return status;

    if ((status = lstf_vm_stack_push_boolean(cr->stack, operand1 && operand2)))
        return status;

    return status;
}

static lstf_vm_status
lstf_vm_op_lor_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    (void) vm;
    lstf_vm_status status = lstf_vm_status_continue;
    bool operand1, operand2;

    if ((status = lstf_vm_stack_pop_boolean(cr->stack, &operand2)))
        return status;

    if ((status = lstf_vm_stack_pop_boolean(cr->stack, &operand1)))
        return status;

    if ((status = lstf_vm_stack_push_boolean(cr->stack, operand1 || operand2)))
        return status;

    return status;
}

static lstf_vm_status
lstf_vm_op_lnot_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    (void) vm;
    lstf_vm_status status = lstf_vm_status_continue;
    bool operand;

    if ((status = lstf_vm_stack_pop_boolean(cr->stack, &operand)))
        return status;

    if ((status = lstf_vm_stack_push_boolean(cr->stack, !operand)))
        return status;

    return status;
}

#define implement_comparison_op(instruction_name, operation) \
static lstf_vm_status \
lstf_vm_op_## instruction_name ## _exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)\
{ \
    (void) vm; \
    lstf_vm_status status = lstf_vm_status_continue; \
    lstf_vm_value operand1, operand2; \
\
    if ((status = lstf_vm_stack_pop_value(cr->stack, &operand2))) \
        return status; \
 \
    if ((status = lstf_vm_stack_pop_value(cr->stack, &operand1))) { \
        lstf_vm_value_clear(&operand2); \
        return status; \
    } \
 \
    if (operand1.value_type != lstf_vm_value_type_integer && \
            operand1.value_type != lstf_vm_value_type_double) { \
        lstf_vm_value_clear(&operand1); \
        lstf_vm_value_clear(&operand2); \
        return lstf_vm_status_invalid_operand_type; \
    } \
 \
    if (operand2.value_type != lstf_vm_value_type_integer && \
            operand2.value_type != lstf_vm_value_type_double) { \
        lstf_vm_value_clear(&operand1); \
        lstf_vm_value_clear(&operand2); \
        return lstf_vm_status_invalid_operand_type; \
    } \
 \
    if (operand1.value_type == lstf_vm_value_type_integer) { \
        if (operand2.value_type == lstf_vm_value_type_integer) { \
            status = lstf_vm_stack_push_boolean(cr->stack, \
                            operand1.data.integer operation operand2.data.integer); \
        } else { \
            status = lstf_vm_stack_push_boolean(cr->stack, \
                            operand1.data.integer operation operand2.data.double_value); \
        } \
    } else { \
        if (operand2.value_type == lstf_vm_value_type_integer) { \
            status = lstf_vm_stack_push_boolean(cr->stack, \
                            operand1.data.double_value operation operand2.data.integer); \
        } else { \
            status = lstf_vm_stack_push_boolean(cr->stack, \
                            operand1.data.double_value operation operand2.data.double_value); \
        } \
    } \
 \
    return status; \
}

implement_comparison_op(lessthan, <)
implement_comparison_op(lessthan_equal, <=)

static lstf_vm_status
lstf_vm_op_equal_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    (void) vm;
    lstf_vm_status status = lstf_vm_status_continue;
    lstf_vm_value operand1, operand2;

    if ((status = lstf_vm_stack_pop_value(cr->stack, &operand2)))
        return status;

    if ((status = lstf_vm_stack_pop_value(cr->stack, &operand1))) {
        lstf_vm_value_clear(&operand2);
        return status;
    }

    if (operand1.value_type == operand2.value_type) {
        switch (operand1.value_type) {
            case lstf_vm_value_type_array_ref:
            case lstf_vm_value_type_object_ref:
            case lstf_vm_value_type_pattern_ref:
                status = lstf_vm_stack_push_boolean(cr->stack,
                                json_node_equal_to(operand1.data.json_node_ref, operand2.data.json_node_ref));
                break;
            case lstf_vm_value_type_boolean:
                status = lstf_vm_stack_push_boolean(cr->stack,
                                operand1.data.boolean == operand2.data.boolean);
                break;
            case lstf_vm_value_type_code_address:
                status = lstf_vm_stack_push_boolean(cr->stack,
                                operand1.data.address == operand2.data.address);
                break;
            case lstf_vm_value_type_double:
                status = lstf_vm_stack_push_boolean(cr->stack,
                                operand1.data.double_value == operand2.data.double_value);
                break;
            case lstf_vm_value_type_integer:
                status = lstf_vm_stack_push_boolean(cr->stack,
                                operand1.data.integer == operand2.data.integer);
                break;
            case lstf_vm_value_type_null:
                status = lstf_vm_stack_push_boolean(cr->stack, true);
                break;
            case lstf_vm_value_type_string:
                status = lstf_vm_stack_push_boolean(cr->stack,
                                string_is_equal_to(operand1.data.string, operand2.data.string));
                break;
            case lstf_vm_value_type_closure:
                status = lstf_vm_stack_push_boolean(cr->stack,
                                lstf_vm_closure_equal_to(operand1.data.closure, operand2.data.closure));
                break;
        }
    } else if (operand1.value_type == lstf_vm_value_type_integer &&
            operand2.value_type == lstf_vm_value_type_double) {
        status = lstf_vm_stack_push_boolean(cr->stack,
                        operand1.data.integer == operand2.data.double_value);
    } else if (operand1.value_type == lstf_vm_value_type_double &&
            operand2.value_type == lstf_vm_value_type_integer) {
        status = lstf_vm_stack_push_boolean(cr->stack,
                        operand1.data.double_value == operand2.data.integer);
    } else if (lstf_vm_value_type_is_json(operand1.value_type) && lstf_vm_value_type_is_json(operand2.value_type)) {
        status = lstf_vm_stack_push_boolean(cr->stack,
                        json_node_equal_to(operand1.data.json_node_ref, operand2.data.json_node_ref));
    } else {
        status = lstf_vm_status_invalid_operand_type;
    }

    lstf_vm_value_clear(&operand1);
    lstf_vm_value_clear(&operand2);
    return status;
}

implement_comparison_op(greaterthan, >)
implement_comparison_op(greaterthan_equal, >=)

#define implement_arithmetic_op(instruction_name, operation) \
static lstf_vm_status \
lstf_vm_op_## instruction_name ## _exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)\
{ \
    (void) vm; \
    lstf_vm_status status = lstf_vm_status_continue; \
    lstf_vm_value operand1, operand2; \
\
    if ((status = lstf_vm_stack_pop_value(cr->stack, &operand2))) \
        return status; \
 \
    if ((status = lstf_vm_stack_pop_value(cr->stack, &operand1))) { \
        lstf_vm_value_clear(&operand2); \
        return status; \
    } \
 \
    if (operand1.value_type != lstf_vm_value_type_integer && \
            operand1.value_type != lstf_vm_value_type_double) { \
        lstf_vm_value_clear(&operand1); \
        lstf_vm_value_clear(&operand2); \
        return lstf_vm_status_invalid_operand_type; \
    } \
 \
    if (operand2.value_type != lstf_vm_value_type_integer && \
            operand2.value_type != lstf_vm_value_type_double) { \
        lstf_vm_value_clear(&operand1); \
        lstf_vm_value_clear(&operand2); \
        return lstf_vm_status_invalid_operand_type; \
    } \
 \
    if (operand1.value_type == lstf_vm_value_type_integer) { \
        if (operand2.value_type == lstf_vm_value_type_integer) { \
            status = lstf_vm_stack_push_integer(cr->stack, \
                            operand1.data.integer operation operand2.data.integer); \
        } else { \
            status = lstf_vm_stack_push_double(cr->stack, \
                            operand1.data.integer operation operand2.data.double_value); \
        } \
    } else { \
        if (operand2.value_type == lstf_vm_value_type_integer) { \
            status = lstf_vm_stack_push_double(cr->stack, \
                            operand1.data.double_value operation operand2.data.integer); \
        } else { \
            status = lstf_vm_stack_push_double(cr->stack, \
                            operand1.data.double_value operation operand2.data.double_value); \
        } \
    } \
 \
    return status; \
}

implement_arithmetic_op(add, +)
implement_arithmetic_op(sub, -)
implement_arithmetic_op(mul, *)
implement_arithmetic_op(div, /)

static lstf_vm_status
lstf_vm_op_pow_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    (void) vm;
    lstf_vm_status status = lstf_vm_status_continue;
    lstf_vm_value operand1, operand2;

    if ((status = lstf_vm_stack_pop_value(cr->stack, &operand2)))
        return status;

    if ((status = lstf_vm_stack_pop_value(cr->stack, &operand1))) {
        lstf_vm_value_clear(&operand2);
        return status;
    }

    if (operand1.value_type != lstf_vm_value_type_integer &&
            operand1.value_type != lstf_vm_value_type_double) {
        lstf_vm_value_clear(&operand1);
        lstf_vm_value_clear(&operand2);
        return lstf_vm_status_invalid_operand_type;
    }

    if (operand2.value_type != lstf_vm_value_type_integer &&
            operand2.value_type != lstf_vm_value_type_double) {
        lstf_vm_value_clear(&operand1);
        lstf_vm_value_clear(&operand2);
        return lstf_vm_status_invalid_operand_type;
    }

    if (operand1.value_type == lstf_vm_value_type_integer) {
        if (operand2.value_type == lstf_vm_value_type_integer) {
            if (operand2.data.integer >= 0)
                status = lstf_vm_stack_push_integer(cr->stack,
                        powint(operand1.data.integer, operand2.data.integer));
            else
                status = lstf_vm_stack_push_double(cr->stack,
                        pow((double)operand1.data.integer, (double)operand2.data.integer));
        } else {
            status = lstf_vm_stack_push_double(cr->stack,
                            pow((double)operand1.data.integer, operand2.data.double_value));
        }
    } else {
        if (operand2.value_type == lstf_vm_value_type_integer) {
            status = lstf_vm_stack_push_double(cr->stack,
                            pow(operand1.data.double_value, (double)operand2.data.integer));
        } else {
            status = lstf_vm_stack_push_double(cr->stack,
                            pow(operand1.data.double_value, operand2.data.double_value));
        }
    }

    return status;
}

static lstf_vm_status
lstf_vm_op_mod_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    (void) vm;
    lstf_vm_status status = lstf_vm_status_continue;
    lstf_vm_value operand1, operand2;

    if ((status = lstf_vm_stack_pop_value(cr->stack, &operand2)))
        return status;

    if ((status = lstf_vm_stack_pop_value(cr->stack, &operand1))) {
        lstf_vm_value_clear(&operand2);
        return status;
    }

    if (!(operand1.value_type == lstf_vm_value_type_integer ||
                operand1.value_type == lstf_vm_value_type_double) &&
            !(operand2.value_type == lstf_vm_value_type_integer ||
                operand2.value_type == lstf_vm_value_type_double)) {
        lstf_vm_value_clear(&operand1);
        lstf_vm_value_clear(&operand2);
        return lstf_vm_status_invalid_operand_type;
    }

    if (operand1.value_type == lstf_vm_value_type_integer) {
        if (operand2.value_type == lstf_vm_value_type_integer) {
            status = lstf_vm_stack_push_integer(cr->stack,
                    operand1.data.integer % operand2.data.integer);
        } else {
            status = lstf_vm_stack_push_double(cr->stack,
                    fmod((double)operand1.data.integer, operand2.data.double_value));
        }
    } else {
        if (operand2.value_type == lstf_vm_value_type_integer) {
            status = lstf_vm_stack_push_double(cr->stack,
                    fmod(operand1.data.double_value, (double)operand2.data.integer));
        } else {
            status = lstf_vm_stack_push_double(cr->stack,
                    fmod(operand1.data.double_value, operand2.data.double_value));
        }
    }

    return status;
}

#define implement_bitwise_op(instruction_name, operation) \
static lstf_vm_status \
lstf_vm_op_## instruction_name ##_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr) \
{ \
    (void) vm; \
    lstf_vm_status status = lstf_vm_status_continue; \
    int64_t operand1, operand2; \
 \
    if ((status = lstf_vm_stack_pop_integer(cr->stack, &operand2))) \
        return status; \
 \
    if ((status = lstf_vm_stack_pop_integer(cr->stack, &operand1))) \
        return status; \
 \
    status = lstf_vm_stack_push_integer(cr->stack, ((uint64_t)operand1) operation ((uint64_t)operand2)); \
 \
    return status; \
}

implement_bitwise_op(and, &)
implement_bitwise_op(or, |)
implement_bitwise_op(xor, ^)
implement_bitwise_op(lshift, <<)
implement_bitwise_op(rshift, >>)

static lstf_vm_status
lstf_vm_op_print_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    lstf_vm_value value;

    if ((status = lstf_vm_stack_pop_value(cr->stack, &value)))
        return status;

    switch (value.value_type) {
        case lstf_vm_value_type_array_ref:
        case lstf_vm_value_type_object_ref:
        case lstf_vm_value_type_pattern_ref:
        {
            char *json_representation = json_node_to_string(value.data.json_node_ref, true);
            outputstream_printf(vm->ostream, "%s\n", json_representation);
            free(json_representation);
        }   break;
        case lstf_vm_value_type_boolean:
            outputstream_printf(vm->ostream, "%s\n", value.data.boolean ? "true" : "false");
            break;
        case lstf_vm_value_type_code_address:
            outputstream_printf(vm->ostream, "[VM code @ 0x%p]\n", (void *)value.data.address);
            break;
        case lstf_vm_value_type_double:
            outputstream_printf(vm->ostream, "%lf\n", value.data.double_value);
            break;
        case lstf_vm_value_type_integer:
            outputstream_printf(vm->ostream, "%"PRIi64"\n", value.data.integer);
            break;
        case lstf_vm_value_type_null:
            outputstream_printf(vm->ostream, "null\n");
            break;
        case lstf_vm_value_type_string:
            outputstream_printf(vm->ostream, "%s\n", value.data.string->buffer);
            break;
        case lstf_vm_value_type_closure:
            outputstream_printf(vm->ostream, "[closure [VM code @ %#"PRIxPTR"] [%u up-values]]",
                    (uintptr_t)(value.data.closure->code_address - vm->program->code),
                    value.data.closure->num_upvalues);
            break;
    }

    lstf_vm_value_clear(&value);
    return status;
}

static lstf_vm_status
lstf_vm_op_exit_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    int8_t return_code;

    if ((status = lstf_virtualmachine_read_signed_byte(vm, cr, &return_code)))
        return status;

    vm->return_code = return_code;
    status = lstf_vm_status_exited;

    return status;
}

static lstf_vm_status
lstf_vm_op_assert_exec(lstf_virtualmachine *vm, lstf_vm_coroutine *cr)
{
    (void) vm;
    lstf_vm_status status = lstf_vm_status_continue;
    bool previous_result;

    if ((status = lstf_vm_stack_pop_boolean(cr->stack, &previous_result)))
        return status;

    if (!previous_result)
        status = lstf_vm_status_assertion_failed;

    return status;
}

static lstf_vm_status (*const instruction_table[256])(lstf_virtualmachine *, lstf_vm_coroutine *) = {
    // --- reading/writing to/from memory
    [lstf_vm_op_load_frameoffset]   = lstf_vm_op_load_frameoffset_exec,
    [lstf_vm_op_load_dataoffset]    = lstf_vm_op_load_dataoffset_exec,
    [lstf_vm_op_load_codeoffset]    = lstf_vm_op_load_codeoffset_exec,
    [lstf_vm_op_load_expression]    = lstf_vm_op_load_expression_exec,
    [lstf_vm_op_store]              = lstf_vm_op_store_exec,
    [lstf_vm_op_pop]                = lstf_vm_op_pop_exec,

    // --- accessing members of a structured type
    [lstf_vm_op_get]                = lstf_vm_op_get_exec,
    [lstf_vm_op_set]                = lstf_vm_op_set_exec,
    [lstf_vm_op_append]             = lstf_vm_op_append_exec,

    // --- functions
    [lstf_vm_op_params]             = lstf_vm_op_params_exec,
    [lstf_vm_op_call]               = lstf_vm_op_call_exec,
    [lstf_vm_op_calli]              = lstf_vm_op_calli_exec,
    [lstf_vm_op_schedule]           = lstf_vm_op_schedule_exec,
    [lstf_vm_op_schedulei]          = lstf_vm_op_schedulei_exec,
    [lstf_vm_op_return]             = lstf_vm_op_return_exec,
    [lstf_vm_op_closure]            = lstf_vm_op_closure_exec,
    [lstf_vm_op_upget]              = lstf_vm_op_upget_exec,
    [lstf_vm_op_upset]              = lstf_vm_op_upset_exec,
    [lstf_vm_op_vmcall]             = lstf_vm_op_vmcall_exec,

    // --- control flow
    [lstf_vm_op_else]               = lstf_vm_op_else_exec,
    [lstf_vm_op_jump]               = lstf_vm_op_jump_exec,

    // --- logical operations
    [lstf_vm_op_bool]               = lstf_vm_op_bool_exec,
    [lstf_vm_op_land]               = lstf_vm_op_land_exec,
    [lstf_vm_op_lor]                = lstf_vm_op_lor_exec,
    [lstf_vm_op_lnot]               = lstf_vm_op_lnot_exec,

    // --- comparison operations
    [lstf_vm_op_lessthan]           = lstf_vm_op_lessthan_exec,
    [lstf_vm_op_lessthan_equal]     = lstf_vm_op_lessthan_equal_exec,
    [lstf_vm_op_equal]              = lstf_vm_op_equal_exec,
    [lstf_vm_op_greaterthan]        = lstf_vm_op_greaterthan_exec,
    [lstf_vm_op_greaterthan_equal]  = lstf_vm_op_greaterthan_equal_exec,

    // --- arithmetic operations
    [lstf_vm_op_add]                = lstf_vm_op_add_exec,
    [lstf_vm_op_sub]                = lstf_vm_op_sub_exec,
    [lstf_vm_op_mul]                = lstf_vm_op_mul_exec,
    [lstf_vm_op_div]                = lstf_vm_op_div_exec,
    [lstf_vm_op_pow]                = lstf_vm_op_pow_exec,
    [lstf_vm_op_mod]                = lstf_vm_op_mod_exec,

    // --- bitwise operations
    [lstf_vm_op_and]                = lstf_vm_op_and_exec,
    [lstf_vm_op_or]                 = lstf_vm_op_or_exec,
    [lstf_vm_op_xor]                = lstf_vm_op_xor_exec,
    [lstf_vm_op_lshift]             = lstf_vm_op_lshift_exec,
    [lstf_vm_op_rshift]             = lstf_vm_op_rshift_exec,

    // --- input/output
    [lstf_vm_op_print]              = lstf_vm_op_print_exec,
    [lstf_vm_op_exit]               = lstf_vm_op_exit_exec,
    
    // --- miscellaneous
    [lstf_vm_op_assert]             = lstf_vm_op_assert_exec
};

bool
lstf_virtualmachine_run(lstf_virtualmachine *vm)
{
    while (true) {
        if (vm->should_stop)
            return true;

        if (!(vm->last_status == lstf_vm_status_continue || vm->last_status == lstf_vm_status_hit_breakpoint))
            return false;

        // initialize the main coroutine if it was never created
        if (!vm->main_coroutine) {
            // initialize main coroutine to program entry point
            lstf_vm_coroutine *main_cr = lstf_vm_coroutine_new(vm->program->entry_point);

            // setup initial stack frame
            if ((vm->last_status = lstf_vm_stack_setup_frame(main_cr->stack, NULL, NULL)))
                return false;

            vm->main_coroutine = lstf_vm_coroutine_ref(main_cr);
            ptr_list_append(vm->run_queue, main_cr);
        }

        if (ptr_list_is_empty(vm->run_queue) && ptr_list_is_empty(vm->suspended_list)) {
            // the main coroutine has exited normally (it is not in the run
            // queue or the suspended list) and there are no other coroutines,
            // so we should stop the virtual machine
            if (!vm->last_status)
                vm->last_status = lstf_vm_status_exited;
            return false;
        }

        if (vm->instructions_executed >= LSTF_VM_CONTEXT_SWITCH_CYCLES) {
            // run one iteration of the event loop on every context switch, and
            // allow blocking if the run queue is empty
            vm->instructions_executed = 0;      // reset instruction counter
            eventloop_process(vm->event_loop, !ptr_list_is_empty(vm->run_queue));
        } else {
            // we don't want to run the event loop every cycle, since that will
            // involve a number of system calls (poll() on POSIX and
            // WaitForMultipleObjects() + CreateThread() and friends on
            // Windows). When it's not time to context switch, we only want to
            // run the event loop if we have no choice because there is nothing
            // in the run queue.
            if (ptr_list_is_empty(vm->run_queue)) {
                // we have no runnable coroutines, so we must wait.
                // vm->suspended_list must be non-empty here (because of the
                // earlier check), so after this event loop iteration finishes
                // we should have at least one runnable coroutine
                eventloop_process(vm->event_loop, false);
                assert(!ptr_list_is_empty(vm->run_queue) && "there must be at least one runnable"
                        " coroutine after a blocking iteration of an event loop!");
            }
        }

        // now pick a runnable coroutine from the head of the queue
        lstf_vm_coroutine *cr = 
            lstf_vm_coroutine_ref(ptr_list_node_get_data(vm->run_queue->head, lstf_vm_coroutine *));

        vm->last_pc = cr->pc;

        // check if we just arrived at a breakpoint for the first time
        if (vm->last_status != lstf_vm_status_hit_breakpoint &&
            vm->debug && ptr_hashset_contains(vm->breakpoints, (void *)(cr->pc - vm->program->entry_point))) {
            vm->last_status = lstf_vm_status_hit_breakpoint;
            lstf_vm_coroutine_unref(cr);
            return true;
        }

        // remove the coroutine from the head of the run queue and decrement its refcount
        ptr_list_remove_first_link(vm->run_queue);
        cr->node = NULL;

        // fetch the instruction
        uint8_t opcode;
        if ((vm->last_status = lstf_virtualmachine_read_byte(vm, cr, &opcode))) {
            // a bad fetch from any coroutine should halt the virtual machine
            lstf_vm_coroutine_unref(cr);
            return false;
        }

        // execute the instruction
        if (instruction_table[opcode]) {
            vm->last_status = instruction_table[opcode](vm, cr);
        } else {
            vm->last_status = lstf_vm_status_invalid_instruction;
        }
        vm->instructions_executed++;

        // decide what to do with the current coroutine: keep it or throw it away?
        if (cr->pc && !cr->node) {
            // only keep the coroutine if it has not completed (and it hasn't
            // already been added onto another list by an instruction)
 
            if (cr->outstanding_io == 0) {
                if (vm->instructions_executed >= LSTF_VM_CONTEXT_SWITCH_CYCLES) {
                    // add to the back of the run queue so that we can pick a
                    // different coroutine on the next cycle
                    cr->node = ptr_list_append(vm->run_queue, cr);
                } else {
                    cr->node = ptr_list_prepend(vm->run_queue, cr);
                }
            } else {
                cr->node = ptr_list_append(vm->suspended_list, cr);
            }
        }
        lstf_vm_coroutine_unref(cr);
    }
}

// --- debugging

bool lstf_virtualmachine_add_breakpoint(lstf_virtualmachine *vm, ptrdiff_t code_offset)
{
    if (code_offset < 0 || (uint64_t)code_offset >= vm->program->code_size)
        return false;
    ptr_hashset_insert(vm->breakpoints, (void *)code_offset);
    return true;
}

void lstf_virtualmachine_delete_breakpoint(lstf_virtualmachine *vm, ptrdiff_t code_offset)
{
    ptr_hashset_delete(vm->breakpoints, (void *)code_offset);
}
