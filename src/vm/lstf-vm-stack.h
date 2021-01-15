#pragma once

#include "lstf-vm-value.h"
#include "lstf-vm-status.h"
#include "json/json.h"
#include <limits.h>
#include <stdint.h>

typedef struct _lstf_vm_stack lstf_vm_stack;
typedef struct _lstf_vm_stackframe lstf_vm_stackframe;

struct _lstf_vm_stack {
    /**
     * The growable value stack.
     */
    lstf_vm_value *values;

    unsigned n_values;
    unsigned values_buffer_size;

    /**
     * Growable array of frame pointers.
     */
    lstf_vm_stackframe *frames;

    unsigned n_frames;
    unsigned frames_buffer_size;
};

struct _lstf_vm_stackframe {
    /**
     * The base of the current stack frame, as an offset within the stack.
     */
    uint64_t offset;

    /**
     * The number of parameters to pop on return.
     */
    uint8_t parameters;
};

lstf_vm_stack *lstf_vm_stack_new(void);

void lstf_vm_stack_destroy(lstf_vm_stack *stack);

// --- observes the stack

lstf_vm_status lstf_vm_stack_get_value(lstf_vm_stack  *stack,
                                       int64_t         fp_offset,
                                       lstf_vm_value  *value);

lstf_vm_status lstf_vm_stack_get_integer(lstf_vm_stack *stack,
                                         int64_t        fp_offset,
                                         int64_t       *value);

lstf_vm_status lstf_vm_stack_get_double(lstf_vm_stack *stack,
                                        int64_t        fp_offset,
                                        double        *value);

lstf_vm_status lstf_vm_stack_get_boolean(lstf_vm_stack *stack,
                                         int64_t        fp_offset,
                                         bool          *value);

lstf_vm_status lstf_vm_stack_get_string(lstf_vm_stack *stack,
                                        int64_t        fp_offset,
                                        string       **value);

lstf_vm_status lstf_vm_stack_get_code_address(lstf_vm_stack *stack,
                                              int64_t        fp_offset,
                                              uint8_t      **value);

lstf_vm_status lstf_vm_stack_get_object(lstf_vm_stack *stack,
                                        int64_t        fp_offset,
                                        json_node    **value);

lstf_vm_status lstf_vm_stack_get_array(lstf_vm_stack *stack,
                                       int64_t        fp_offset,
                                       json_node    **value);

lstf_vm_status lstf_vm_stack_get_pattern(lstf_vm_stack *stack,
                                         int64_t        fp_offset,
                                         json_node    **value);

// --- writing before the last values on the stack

/**
 * Sets the stack value at `fp_offset` to `*value`. Modifies the value pointed
 * at to no longer take ownership of the underlying reference (if there is
 * one).
 */
lstf_vm_status lstf_vm_stack_set_value(lstf_vm_stack *stack,
                                       int64_t        fp_offset,
                                       lstf_vm_value *value);

// --- popping the last value off of the stack

/**
 * You can pass in NULL for `value`
 */
lstf_vm_status lstf_vm_stack_pop_value(lstf_vm_stack *stack,
                                       lstf_vm_value *value);

lstf_vm_status lstf_vm_stack_pop_integer(lstf_vm_stack *stack,
                                         int64_t       *value);

lstf_vm_status lstf_vm_stack_pop_double(lstf_vm_stack *stack,
                                        double        *value);

lstf_vm_status lstf_vm_stack_pop_boolean(lstf_vm_stack *stack,
                                        bool          *value);

lstf_vm_status lstf_vm_stack_pop_string(lstf_vm_stack *stack,
                                        string       **value);

lstf_vm_status lstf_vm_stack_pop_code_address(lstf_vm_stack *stack,
                                              uint8_t      **value);

lstf_vm_status lstf_vm_stack_pop_object(lstf_vm_stack *stack,
                                        json_node    **value);

lstf_vm_status lstf_vm_stack_pop_array(lstf_vm_stack *stack,
                                       json_node    **value);

lstf_vm_status lstf_vm_stack_pop_pattern(lstf_vm_stack *stack,
                                         json_node    **value);

// --- pushing a new value onto the stack

/**
 * Pushes a new value onto the stack.
 *
 * Modifies the value pointed to by `value` so that it no longer takes
 * ownership of the underlying thing.
 */
lstf_vm_status lstf_vm_stack_push_value(lstf_vm_stack *stack,
                                        lstf_vm_value *value);

lstf_vm_status lstf_vm_stack_push_integer(lstf_vm_stack *stack,
                                          int64_t        value);

lstf_vm_status lstf_vm_stack_push_double(lstf_vm_stack *stack,
                                         double         value);

lstf_vm_status lstf_vm_stack_push_boolean(lstf_vm_stack *stack,
                                          bool           value);

lstf_vm_status lstf_vm_stack_push_string(lstf_vm_stack *stack,
                                         string        *value);

lstf_vm_status lstf_vm_stack_push_code_address(lstf_vm_stack *stack,
                                               uint8_t       *value);

lstf_vm_status lstf_vm_stack_push_null(lstf_vm_stack *stack);

lstf_vm_status lstf_vm_stack_push_object(lstf_vm_stack *stack,
                                         json_node     *value);

lstf_vm_status lstf_vm_stack_push_array(lstf_vm_stack *stack,
                                        json_node     *value);

lstf_vm_status lstf_vm_stack_push_pattern(lstf_vm_stack *stack,
                                          json_node     *value);

// --- calling and returning from functions

/**
 * Pops the last stack frame from the stack frame list. Will pop off the number
 * of parameters previously established for this stack frame by the `params`
 * instruction. Will check that the current local stack frame is "empty" (that
 * is, the stack is back to where the current frame pointer is). This is used
 * by the `return` instruction.
 *
 * The `return_address` is where the saved return address is written to.
 */
lstf_vm_status lstf_vm_stack_teardown_frame(lstf_vm_stack *stack,
                                            uint8_t      **return_address);

/**
 * Establishes the number of parameters that will be popped when the current
 * stack frame is torn down.
 */
lstf_vm_status lstf_vm_stack_frame_set_parameters(lstf_vm_stack *stack,
                                                  uint8_t        parameters);

/**
 * Establishes a new local stack frame pointing past the last value on the
 * stack.  Essentially, pushes the current stack pointer onto the frame
 * pointers stack.  This is used by the `call` instruction.
 *
 * The `return_address` is saved.
 */
lstf_vm_status lstf_vm_stack_setup_frame(lstf_vm_stack *stack,
                                         uint8_t       *return_address);
