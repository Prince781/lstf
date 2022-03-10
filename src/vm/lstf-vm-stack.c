#include "lstf-vm-stack.h"
#include "lstf-vm-status.h"
#include "lstf-vm-value.h"
#include "util.h"
#include <assert.h>
#include <stdbool.h>
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
    for (unsigned i = 0; i < stack->n_values; i++) {
        bool is_captured = stack->values[i].is_captured;
        stack->values[i].is_captured = false;
        lstf_vm_value_clear(&stack->values[i]);
        stack->values[i].is_captured = is_captured;
    }
    for (unsigned i = 0; i < stack->n_frames; i++) {
        if (stack->frames[i].captured_locals)
            ptr_hashmap_destroy(stack->frames[i].captured_locals);
        lstf_vm_closure_unref(stack->frames[i].closure);
    }
    free(stack->values);
    free(stack->frames);
    free(stack);
}

lstf_vm_status lstf_vm_stack_get_value(lstf_vm_stack *stack,
                                       uint64_t       stack_offset,
                                       lstf_vm_value *value)
{
    if (stack->n_values == 0 || stack_offset >= stack->n_values)
        return lstf_vm_status_invalid_stack_offset;

    *value = stack->values[stack_offset];
    value->takes_ownership = false;
    value->is_captured = false;
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_get_frame_return_address(lstf_vm_stack *stack,
                                                      uint8_t      **return_address_ptr)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_stack_offset;
    *return_address_ptr = stack->frames[stack->n_frames - 1].return_address;
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_frame_get_value(lstf_vm_stack  *stack,
                                             int64_t         fp_offset,
                                             lstf_vm_value  *value)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_stack_offset;
    
    if (stack->frames[stack->n_frames - 1].offset + fp_offset >= stack->n_values)
        return lstf_vm_status_invalid_stack_offset;

    lstf_vm_value *stack_pointer = &stack->values[stack->frames[stack->n_frames - 1].offset + fp_offset];

    *value = *stack_pointer;
    value->takes_ownership = false;
    value->is_captured = false;
    return lstf_vm_status_continue;
}

static inline lstf_vm_status lstf_vm_stack_get_typed_value(lstf_vm_stack     *stack,
                                                           int64_t            fp_offset,
                                                           lstf_vm_value_type value_type,
                                                           lstf_vm_value     *value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_frame_get_value(stack, fp_offset, &generic_value)))
        return status;

    if (generic_value.value_type != value_type)
        return lstf_vm_status_invalid_operand_type;

    *value = generic_value;
    value->takes_ownership = false;
    value->is_captured = false;
    return status;
}

lstf_vm_status lstf_vm_stack_frame_get_value_address(lstf_vm_stack  *stack,
                                                     int64_t         fp_offset,
                                                     lstf_vm_value **value_ptr)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_stack_offset;
    
    if (stack->frames[stack->n_frames - 1].offset + fp_offset >= stack->n_values)
        return lstf_vm_status_invalid_stack_offset;

    *value_ptr = &stack->values[stack->frames[stack->n_frames - 1].offset + fp_offset];
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_frame_get_integer(lstf_vm_stack *stack,
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

lstf_vm_status lstf_vm_stack_frame_get_double(lstf_vm_stack *stack,
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

lstf_vm_status lstf_vm_stack_frame_get_boolean(lstf_vm_stack *stack,
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

lstf_vm_status lstf_vm_stack_frame_get_string(lstf_vm_stack *stack,
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

lstf_vm_status lstf_vm_stack_frame_get_code_address(lstf_vm_stack *stack,
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

lstf_vm_status lstf_vm_stack_frame_get_object(lstf_vm_stack *stack,
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

lstf_vm_status lstf_vm_stack_frame_get_array(lstf_vm_stack *stack,
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

lstf_vm_status lstf_vm_stack_frame_get_pattern(lstf_vm_stack *stack,
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
                                       uint64_t       stack_offset,
                                       lstf_vm_value *value)
{
    if (stack->n_values == 0 || stack_offset >= stack->n_values)
        return lstf_vm_status_invalid_stack_offset;

    lstf_vm_value *stack_pointer = &stack->values[stack_offset];

    lstf_vm_value_clear(stack_pointer);
    *stack_pointer = lstf_vm_value_take_ownership(value);

    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_set_frame_value(lstf_vm_stack *stack,
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

static inline void
lstf_vm_value_uplift(lstf_vm_stack      *stack,
                     lstf_vm_value      *value,
                     lstf_vm_stackframe *frame)
{
    assert(frame->captured_locals &&
            "attempting to uplift local, but no up-values have been created!");
    const ptr_hashmap_entry *sp_uv_pair =
        ptr_hashmap_get(frame->captured_locals, (void *)(uintptr_t)(value - stack->values));
    assert(sp_uv_pair && "captured local value must map to a corresponding up-value!");
    lstf_vm_upvalue *upvalue = sp_uv_pair->value;

    value->is_captured = false;
    upvalue->value = *value;
    upvalue->is_local = false;

    switch (upvalue->value.value_type) {
        case lstf_vm_value_type_integer:
        case lstf_vm_value_type_double:
        case lstf_vm_value_type_boolean:
        case lstf_vm_value_type_code_address:
        case lstf_vm_value_type_null:
            break;
        case lstf_vm_value_type_string:
            string_ref(upvalue->value.data.string);
            break;
        case lstf_vm_value_type_array_ref:
        case lstf_vm_value_type_object_ref:
        case lstf_vm_value_type_pattern_ref:
            json_node_ref(upvalue->value.data.json_node_ref);
            break;
        case lstf_vm_value_type_closure:
            lstf_vm_closure_ref(upvalue->value.data.closure);
            break;
    }
}

lstf_vm_status lstf_vm_stack_pop_value(lstf_vm_stack *stack,
                                       lstf_vm_value *value)
{
    if (stack->n_values == 0 || stack->n_frames == 0)
        return lstf_vm_status_invalid_stack_offset;
    
    if (stack->n_values - 1 < stack->frames[stack->n_frames - 1].offset)
        return lstf_vm_status_frame_underflow;

    lstf_vm_value *stack_pointer = &stack->values[stack->n_values - 1];

    // determine whether we should uplift this value
    if (stack_pointer->is_captured)
        lstf_vm_value_uplift(stack, stack_pointer, &stack->frames[stack->n_frames - 1]);

    if (value)
        *value = lstf_vm_value_take_ownership(stack_pointer);
    else
        lstf_vm_value_clear(stack_pointer);
    stack->n_values--;

    return lstf_vm_status_continue;
}

static inline lstf_vm_status
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

    if (stack_pointer->is_captured)
        lstf_vm_value_uplift(stack, stack_pointer, &stack->frames[stack->n_frames - 1]);
    
    *value = lstf_vm_value_take_ownership(stack_pointer);
    stack->n_values--;

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

lstf_vm_status lstf_vm_stack_pop_closure(lstf_vm_stack    *stack,
                                         lstf_vm_closure **value)
{
    lstf_vm_value generic_value;
    lstf_vm_status status = lstf_vm_status_continue;

    if ((status = lstf_vm_stack_pop_typed_value(stack,
                                                lstf_vm_value_type_closure,
                                                &generic_value)))
        return status;
    
    *value = lstf_vm_closure_ref(generic_value.data.closure);
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
        .is_captured = false,
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
        .is_captured = false,
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
        .is_captured = false,
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
        .is_captured = false,
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
        .is_captured = false,
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
        .is_captured = false,
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
        .is_captured = false,
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
        .is_captured = false,
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
        .is_captured = false,
        .data = { .json_node_ref = json_node_ref(value) }
    };
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_push_closure(lstf_vm_stack   *stack,
                                          lstf_vm_closure *closure)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_push;

    if (stack->n_values >= stack->values_buffer_size)
        return lstf_vm_status_stack_overflow;

    stack->values[stack->n_values++] = (lstf_vm_value) {
        .value_type = lstf_vm_value_type_closure,
        .takes_ownership = true,
        .is_captured = false,
        .data = { .closure = lstf_vm_closure_ref(closure) }
    };

    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_teardown_frame(lstf_vm_stack *stack,
                                            uint8_t      **return_address_ptr)
{
    lstf_vm_status status = lstf_vm_status_continue;
    lstf_vm_value return_value;
    bool has_return_value = false;

    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_return;

    lstf_vm_stackframe *current_frame = &stack->frames[stack->n_frames - 1];
    uint64_t locals = stack->n_values - current_frame->offset;
    
    if (locals > current_frame->parameters) {
        // fetch the top item on the current frame as the return value
        if ((status = lstf_vm_stack_pop_value(stack, &return_value)))
            return status;
        has_return_value = true;
        locals--;

        while (locals > current_frame->parameters) {
            if ((status = lstf_vm_stack_pop_value(stack, NULL))) {
                lstf_vm_value_clear(&return_value);
                return status;
            }
            locals--;
        }
    }

    // pop all parameters
    for (uint8_t i = 0; i < current_frame->parameters; i++) {
        if ((status = lstf_vm_stack_pop_value(stack, NULL))) {
            if (has_return_value)
                lstf_vm_value_clear(&return_value);
            return status;
        }
    }

    // dereference closure
    lstf_vm_closure_unref(current_frame->closure);

    // destroy captured locals hashmap
    if (current_frame->captured_locals)
        ptr_hashmap_destroy(current_frame->captured_locals);
    
    // get return address
    uint8_t *return_address = current_frame->return_address;
    if (return_address_ptr) {
        *return_address_ptr = return_address;
    }

    // officially tear down frame (affects the next pop() calls)
    stack->n_frames--;

    // If the return address is NULL, this function was invoked as a
    // new coroutine.
    if (return_address) {
        // callee pops parameters in previous stack frame
        for (uint8_t i = 0; i < current_frame->parameters; i++) {
            if ((status = lstf_vm_stack_pop_value(stack, NULL))) {
                if (has_return_value)
                    lstf_vm_value_clear(&return_value);
                return status;
            }
        }

        // push return value on previous frame
        if (has_return_value) {
            if ((status = lstf_vm_stack_push_value(stack, &return_value))) {
                lstf_vm_value_clear(&return_value);
                return status;
            }
        }
    }

    return status;
}

lstf_vm_status lstf_vm_stack_frame_set_parameters(lstf_vm_stack *stack,
                                                  uint8_t        parameters)
{
    if (stack->n_frames == 0 || stack->frames[stack->n_frames - 1].parameters > 0)
        return lstf_vm_status_invalid_params;

    stack->frames[stack->n_frames - 1].parameters = parameters;
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_setup_frame(lstf_vm_stack   *stack,
                                         uint8_t         *return_address,
                                         lstf_vm_closure *closure)
{
    if (stack->n_frames >= stack->frames_buffer_size)
        return lstf_vm_status_stack_overflow;
    
    stack->frames[stack->n_frames++] = (lstf_vm_stackframe) {
        .offset = stack->n_values,
        .return_address = return_address,
        .closure = lstf_vm_closure_ref(closure),
        .captured_locals = NULL /* create this hashmap on demand */,
        .parameters = 0
    };

    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_frame_get_upvalue(lstf_vm_stack    *stack,
                                               uint8_t           upvalue_id,
                                               lstf_vm_upvalue **upvalue)
{
    if (stack->n_frames == 0)
        return lstf_vm_status_invalid_stack_offset;

    lstf_vm_stackframe *current_frame = &stack->frames[stack->n_frames - 1];

    if (!current_frame->closure || upvalue_id >= current_frame->closure->num_upvalues)
        return lstf_vm_status_invalid_upvalue;

    *upvalue = current_frame->closure->upvalues[upvalue_id];
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_frame_track_upvalue(lstf_vm_stack   *stack,
                                                 int64_t          fp_offset,
                                                 lstf_vm_upvalue *upvalue)
{
    if (stack->n_frames == 0 || stack->n_values == 0)
        return lstf_vm_status_invalid_stack_offset;

    const uint64_t value_offset = stack->frames[stack->n_frames - 1].offset + fp_offset;
    if (value_offset >= stack->n_values)
        return lstf_vm_status_invalid_stack_offset;

    if (fp_offset < 0)
        // we can't track up-values for stack elements that don't belong to the
        // current frame
        // TODO: define a new lstf_vm_status for this condition
        return lstf_vm_status_invalid_stack_offset;

    lstf_vm_stackframe *current_frame = &stack->frames[stack->n_frames - 1];

    if (!current_frame->captured_locals) {
        current_frame->captured_locals = ptr_hashmap_new(ptrhash,
                NULL, NULL,
                NULL,
                (collection_item_ref_func) lstf_vm_upvalue_ref, (collection_item_unref_func) lstf_vm_upvalue_unref);
    }

    ptr_hashmap_insert(current_frame->captured_locals, (void *)(uintptr_t) value_offset, upvalue);
    stack->values[value_offset].is_captured = true;
    return lstf_vm_status_continue;
}

lstf_vm_status lstf_vm_stack_frame_get_tracked_upvalue(lstf_vm_stack    *stack,
                                                       int64_t           fp_offset,
                                                       lstf_vm_upvalue **upvalue)
{
    if (stack->n_frames == 0 || stack->n_values == 0)
        return lstf_vm_status_invalid_stack_offset;

    const uint64_t value_offset = stack->frames[stack->n_frames - 1].offset + fp_offset;
    if (value_offset >= stack->n_values)
        return lstf_vm_status_invalid_stack_offset;

    if (fp_offset < 0)
        // we never track up-values for stack elements that don't belong to the
        // current frame
        // TODO: define a new lstf_vm_status for this condition
        return lstf_vm_status_invalid_stack_offset;


    lstf_vm_stackframe *current_frame = &stack->frames[stack->n_frames - 1];

    const ptr_hashmap_entry *sp_uv_pair = NULL;
    if (!current_frame->captured_locals ||
            !(sp_uv_pair = ptr_hashmap_get(current_frame->captured_locals, (void *)(uintptr_t) value_offset)))
        return lstf_vm_status_invalid_upvalue;

    *upvalue = sp_uv_pair->value;
    return lstf_vm_status_continue;
}
