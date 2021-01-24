#include "lstf-vm-stack.h"
#include "vm/lstf-vm-status.h"
#include "vm/lstf-vm-value.h"
#include <stdlib.h>
#include <stdio.h>

static bool
lstf_vm_stack_resize_frame_pointers(lstf_vm_stack *stack,
                     unsigned       min_frames_buffer_size)
{

    if (stack->frames_buffer_size < min_frames_buffer_size) {
        lstf_vm_stackframe *new_frames = realloc(
                stack->frames,
                sizeof(*stack->frames) * min_frames_buffer_size 
        );

        if (!new_frames)
            return false;

        stack->frames = new_frames;
        stack->frames_buffer_size = min_frames_buffer_size;
    }

    return true;
}

static bool
lstf_vm_stack_resize_values(lstf_vm_stack *stack,
                            unsigned min_values_buffer_size)
{
    if (stack->values_buffer_size < min_values_buffer_size) {
        lstf_vm_value *new_values = 
            realloc(stack->values, sizeof(*stack->values) * min_values_buffer_size);

        if (!new_values)
            return false;

        stack->values = new_values;
        stack->values_buffer_size = min_values_buffer_size;
    }

    return true;
}

lstf_vm_stack *lstf_vm_stack_new(void)
{
    lstf_vm_stack *stack = calloc(1, sizeof *stack);

    if (!stack) {
        perror("failed to create lstf_vm_stack");
        abort();
    }

    // use 1 MB for each
    if (!lstf_vm_stack_resize_values(stack, 1024 * 1024 / sizeof(*stack->values)) ||
            !lstf_vm_stack_resize_frame_pointers(stack, 1024 * 1024 / sizeof(*stack->frames))) {
        free(stack->values);
        free(stack->frames);
        free(stack);
        stack = NULL;
    }

    return stack;
}

void lstf_vm_stack_destroy(lstf_vm_stack *stack)
{
    for (unsigned i = 0; i < stack->n_values; i++)
        lstf_vm_value_clear(&stack->values[i]);
    free(stack->values);
    free(stack->frames);
    free(stack);
}

lstf_vm_status lstf_vm_stack_get_value(lstf_vm_stack  *stack,
                                       int64_t         fp_offset,
                                       lstf_vm_value  *value)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_stack_offset;

    
    if (stack->frames[stack->n_frames - 1].offset + fp_offset >= stack->n_values)
        return lstf_vm_status_invalid_stack_offset;

    lstf_vm_value *stack_pointer = &stack->values[stack->frames[stack->n_frames - 1].offset + fp_offset];

    *value = *stack_pointer;
    return lstf_vm_status_continue;
}

static inline lstf_vm_status lstf_vm_stack_get_typed_value(lstf_vm_stack     *stack,
                                                           int64_t            fp_offset,
                                                           lstf_vm_value_type value_type,
                                                           lstf_vm_value     *value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_get_value(stack, fp_offset, &generic_value)))
        return status;

    if (generic_value.value_type != value_type)
        return lstf_vm_status_invalid_operand_type;

    *value = generic_value;
    return status;
}

lstf_vm_status lstf_vm_stack_get_integer(lstf_vm_stack *stack,
                                         int64_t        fp_offset,
                                         int64_t       *value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_get_typed_value(stack,
                                                fp_offset,
                                                lstf_vm_value_type_integer,
                                                &generic_value)))
        return status;

    *value = generic_value.data.integer;
    return status;
}

lstf_vm_status lstf_vm_stack_get_double(lstf_vm_stack *stack,
                                        int64_t        fp_offset,
                                        double        *value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_get_typed_value(stack,
                                                fp_offset,
                                                lstf_vm_value_type_double,
                                                &generic_value)))
        return status;

    *value = generic_value.data.double_value;
    return status;
}

lstf_vm_status lstf_vm_stack_get_boolean(lstf_vm_stack *stack,
                                         int64_t        fp_offset,
                                         bool          *value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_get_typed_value(stack,
                                                fp_offset,
                                                lstf_vm_value_type_boolean,
                                                &generic_value)))
        return status;

    *value = generic_value.data.boolean;
    return status;
}

lstf_vm_status lstf_vm_stack_get_string(lstf_vm_stack *stack,
                                        int64_t        fp_offset,
                                        string       **value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_get_typed_value(stack,
                                                fp_offset,
                                                lstf_vm_value_type_string,
                                                &generic_value)))
        return status;

    *value = generic_value.data.string;
    return status;
}

lstf_vm_status lstf_vm_stack_get_code_address(lstf_vm_stack *stack,
                                              int64_t        fp_offset,
                                              uint8_t      **value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_get_typed_value(stack,
                                                fp_offset,
                                                lstf_vm_value_type_code_address,
                                                &generic_value)))
        return status;

    *value = generic_value.data.address;
    return status;
}

lstf_vm_status lstf_vm_stack_get_object(lstf_vm_stack *stack,
                                        int64_t        fp_offset,
                                        json_node    **value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_get_typed_value(stack,
                                                fp_offset,
                                                lstf_vm_value_type_object_ref,
                                                &generic_value)))
        return status;

    *value = generic_value.data.json_node_ref;
    return status;
}

lstf_vm_status lstf_vm_stack_get_array(lstf_vm_stack *stack,
                                       int64_t        fp_offset,
                                       json_node    **value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_get_typed_value(stack,
                                                fp_offset,
                                                lstf_vm_value_type_array_ref,
                                                &generic_value)))
        return status;

    *value = generic_value.data.json_node_ref;
    return status;
}

lstf_vm_status lstf_vm_stack_get_pattern(lstf_vm_stack *stack,
                                         int64_t        fp_offset,
                                         json_node    **value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_get_typed_value(stack,
                                                fp_offset,
                                                lstf_vm_value_type_pattern_ref,
                                                &generic_value)))
        return status;

    *value = generic_value.data.json_node_ref;
    return status;
}

lstf_vm_status lstf_vm_stack_set_value(lstf_vm_stack *stack,
                                       int64_t        fp_offset,
                                       lstf_vm_value *value)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_stack_offset;
    
    if (stack->frames[stack->n_frames - 1].offset + fp_offset >= stack->n_values)
        return lstf_vm_status_invalid_stack_offset;

    lstf_vm_value *stack_pointer = &stack->values[stack->frames[stack->n_frames - 1].offset + fp_offset];
    
    lstf_vm_value_clear(stack_pointer);
    *stack_pointer = lstf_vm_value_take_ownership(value);
    
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_pop_value(lstf_vm_stack *stack,
                                       lstf_vm_value *value)
{
    if (stack->n_values == 0 || stack->n_frames == 0)
        return lstf_vm_status_invalid_stack_offset;
    
    if (stack->n_values - 1 < stack->frames[stack->n_frames - 1].offset)
        return lstf_vm_status_frame_underflow;

    lstf_vm_value *stack_pointer = &stack->values[stack->n_values - 1];

    if (value)
        *value = *stack_pointer;
    stack->n_values--;

    // TODO: resize the stack if buffer's free space is greater than 50% ???

    return lstf_vm_status_continue;
}

static lstf_vm_status
lstf_vm_stack_pop_typed_value(lstf_vm_stack     *stack,
                              lstf_vm_value_type value_type,
                              lstf_vm_value     *value)
{
    if (stack->n_values == 0 || stack->n_frames == 0)
        return lstf_vm_status_invalid_stack_offset;
    
    if (stack->n_values - 1 < stack->frames[stack->n_frames - 1].offset)
        return lstf_vm_status_frame_underflow;

    lstf_vm_value *stack_pointer = &stack->values[stack->n_values - 1];
    
    if (stack_pointer->value_type != value_type)
        return lstf_vm_status_invalid_operand_type;
    
    *value = *stack_pointer;
    stack->n_values--;

    // TODO: resize the stack if buffer's free space is greater than 50% ???

    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_pop_integer(lstf_vm_stack *stack,
                                         int64_t       *value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_pop_typed_value(stack,
                                                lstf_vm_value_type_integer,
                                                &generic_value)))
        return status;
    
    *value = generic_value.data.integer;
    lstf_vm_value_clear(&generic_value);
    return status;
}

lstf_vm_status lstf_vm_stack_pop_double(lstf_vm_stack *stack,
                                        double        *value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_pop_typed_value(stack,
                                                lstf_vm_value_type_double,
                                                &generic_value)))
        return status;
    
    *value = generic_value.data.double_value;
    lstf_vm_value_clear(&generic_value);
    return status;
}

lstf_vm_status lstf_vm_stack_pop_boolean(lstf_vm_stack *stack,
                                         bool          *value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_pop_typed_value(stack,
                                                lstf_vm_value_type_boolean,
                                                &generic_value)))
        return status;
    
    *value = generic_value.data.boolean;
    lstf_vm_value_clear(&generic_value);
    return status;
}

lstf_vm_status lstf_vm_stack_pop_string(lstf_vm_stack *stack,
                                        string       **value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_pop_typed_value(stack,
                                                lstf_vm_value_type_string,
                                                &generic_value)))
        return status;
    
    *value = string_ref(generic_value.data.string);
    lstf_vm_value_clear(&generic_value);
    return status;
}

lstf_vm_status lstf_vm_stack_pop_code_address(lstf_vm_stack *stack,
                                              uint8_t      **value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_pop_typed_value(stack,
                                                lstf_vm_value_type_code_address,
                                                &generic_value)))
        return status;
    
    *value = generic_value.data.address;
    lstf_vm_value_clear(&generic_value);
    return status;
}

lstf_vm_status lstf_vm_stack_pop_object(lstf_vm_stack *stack,
                                        json_node    **value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_pop_typed_value(stack,
                                                lstf_vm_value_type_object_ref,
                                                &generic_value)))
        return status;
    
    *value = json_node_ref(generic_value.data.json_node_ref);
    lstf_vm_value_clear(&generic_value);
    return status;
}

lstf_vm_status lstf_vm_stack_pop_array(lstf_vm_stack *stack,
                                       json_node    **value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_pop_typed_value(stack,
                                                lstf_vm_value_type_array_ref,
                                                &generic_value)))
        return status;
    
    *value = json_node_ref(generic_value.data.json_node_ref);
    lstf_vm_value_clear(&generic_value);
    return status;
}

lstf_vm_status lstf_vm_stack_pop_pattern(lstf_vm_stack *stack,
                                         json_node    **value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_pop_typed_value(stack,
                                                lstf_vm_value_type_pattern_ref,
                                                &generic_value)))
        return status;
    
    *value = json_node_ref(generic_value.data.json_node_ref);
    lstf_vm_value_clear(&generic_value);
    return status;
}

lstf_vm_status lstf_vm_stack_push_value(lstf_vm_stack *stack,
                                        lstf_vm_value *value)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_push;
    
    if (stack->n_values >= stack->values_buffer_size)
        return lstf_vm_status_stack_overflow;
    
    stack->values[stack->n_values++] = lstf_vm_value_take_ownership(value);
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_push_integer(lstf_vm_stack *stack,
                                          int64_t        value)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_push;

    if (stack->n_values >= stack->values_buffer_size)
        return lstf_vm_status_stack_overflow;

    stack->values[stack->n_values++] = (lstf_vm_value) {
        .value_type = lstf_vm_value_type_integer,
        .takes_ownership = true,
        .data = { .integer = value }
    };
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_push_double(lstf_vm_stack *stack,
                                         double         value)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_push;

    if (stack->n_values >= stack->values_buffer_size)
        return lstf_vm_status_stack_overflow;

    stack->values[stack->n_values++] = (lstf_vm_value) {
        .value_type = lstf_vm_value_type_double,
        .takes_ownership = true,
        .data = { .double_value = value }
    };
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_push_boolean(lstf_vm_stack *stack,
                                          bool           value)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_push;

    if (stack->n_values >= stack->values_buffer_size)
        return lstf_vm_status_stack_overflow;

    stack->values[stack->n_values++] = (lstf_vm_value) {
        .value_type = lstf_vm_value_type_boolean,
        .takes_ownership = true,
        .data = { .boolean = value }
    };
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_push_string(lstf_vm_stack *stack,
                                         string        *value)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_push;

    if (stack->n_values >= stack->values_buffer_size)
        return lstf_vm_status_stack_overflow;

    stack->values[stack->n_values++] = (lstf_vm_value) {
        .value_type = lstf_vm_value_type_string,
        .takes_ownership = true,
        .data = { .string = string_ref(value) }
    };
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_push_code_address(lstf_vm_stack *stack,
                                               uint8_t       *value)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_push;

    if (stack->n_values >= stack->values_buffer_size)
        return lstf_vm_status_stack_overflow;

    stack->values[stack->n_values++] = (lstf_vm_value) {
        .value_type = lstf_vm_value_type_code_address,
        .takes_ownership = true,
        .data = { .address = value }
    };
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_push_null(lstf_vm_stack *stack)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_push;

    if (stack->n_values >= stack->values_buffer_size)
        return lstf_vm_status_stack_overflow;

    stack->values[stack->n_values++] = (lstf_vm_value) {
        .value_type = lstf_vm_value_type_null,
        .takes_ownership = true,
        .data = { .address = 0 }
    };
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_push_object(lstf_vm_stack *stack,
                                         json_node     *value)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_push;

    if (stack->n_values >= stack->values_buffer_size)
        return lstf_vm_status_stack_overflow;

    stack->values[stack->n_values++] = (lstf_vm_value) {
        .value_type = lstf_vm_value_type_object_ref,
        .takes_ownership = true,
        .data = { .json_node_ref = json_node_ref(value) }
    };
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_push_array(lstf_vm_stack *stack,
                                        json_node     *value)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_push;

    if (stack->n_values >= stack->values_buffer_size)
        return lstf_vm_status_stack_overflow;

    stack->values[stack->n_values++] = (lstf_vm_value) {
        .value_type = lstf_vm_value_type_array_ref,
        .takes_ownership = true,
        .data = { .json_node_ref = json_node_ref(value) }
    };
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_push_pattern(lstf_vm_stack *stack,
                                          json_node     *value)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_push;

    if (stack->n_values >= stack->values_buffer_size)
        return lstf_vm_status_stack_overflow;

    stack->values[stack->n_values++] = (lstf_vm_value) {
        .value_type = lstf_vm_value_type_pattern_ref,
        .takes_ownership = true,
        .data = { .json_node_ref = json_node_ref(value) }
    };
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_teardown_frame(lstf_vm_stack *stack,
                                            uint8_t      **return_address)
{
    lstf_vm_status status = lstf_vm_status_continue;
    lstf_vm_value return_value;
    bool has_return_value = false;

    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_return;

    lstf_vm_stackframe *current_frame = &stack->frames[stack->n_frames - 1];
    uint64_t locals = stack->n_values - current_frame->offset;
    if (locals > (uint64_t)1 /* return address */ + current_frame->parameters + 1 /* possible return value */)
        return lstf_vm_status_invalid_return;
    
    if (locals == (uint64_t)1 + current_frame->parameters + 1) {
        if ((status = lstf_vm_stack_pop_value(stack, &return_value)))
            return status;
        has_return_value = true;
        locals--;
    }

    // pop all parameters
    for (uint8_t i = 0; i < current_frame->parameters; i++)
        if ((status = lstf_vm_stack_pop_value(stack, NULL)))
            return status;
    
    // pop code address
    if ((status = lstf_vm_stack_pop_code_address(stack, return_address)))
        return status;

    // officially tear down frame (affects the next pop() calls)
    stack->n_frames--;

    // callee pops parameters in previous stack frame
    for (uint8_t i = 0; i < current_frame->parameters; i++)
        if ((status = lstf_vm_stack_pop_value(stack, NULL)))
            return status;

    // push return value on previous frame
    if (has_return_value)
        if ((status = lstf_vm_stack_push_value(stack, &return_value)))
            return status;

    return status;
}

lstf_vm_status lstf_vm_stack_frame_set_parameters(lstf_vm_stack *stack,
                                                  uint8_t        parameters)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_params;

    stack->frames[stack->n_frames - 1].parameters = parameters;
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_setup_frame(lstf_vm_stack *stack,
                                         uint8_t       *return_address)
{
    if (stack->n_frames >= stack->frames_buffer_size)
        return lstf_vm_status_stack_overflow;
    
    stack->frames[stack->n_frames++] = (lstf_vm_stackframe) {
        .offset = stack->n_values,
        .parameters = 0
    };

    return lstf_vm_stack_push_code_address(stack, return_address);
}
