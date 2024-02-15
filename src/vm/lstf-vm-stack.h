#pragma once

#include "data-structures/ptr-hashmap.h"
#include "lstf-vm-value.h"
#include "lstf-vm-status.h"
#include "json/json.h"
#include <limits.h>
#include <stdint.h>

typedef struct _lstf_vm_stack lstf_vm_stack;
/**
 * A stack frame / activation record
 */
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
    uint64_t offset : 56;

    /**
     * The number of parameters to pop on return.
     */
    uint8_t parameters;

    /**
     * The location to jump back to when this function returns.
     */
    uint8_t *return_address;

    /**
     * Information about the closure we are executing. Can be NULL if the
     * current function is not a closure.
     */
    lstf_vm_closure *closure;

    /**
     * Maps `(offset: uintptr_t coerced to (void *)) -> (lstf_vm_upvalue *)`
     */
    ptr_hashmap *captured_locals;
};

lstf_vm_stack *lstf_vm_stack_new(void);

void lstf_vm_stack_destroy(lstf_vm_stack *stack);

// --- observes the stack

/**
 * Gets a value from an absolute position on the stack.
 */
lstf_vm_status lstf_vm_stack_get_value(lstf_vm_stack *stack,
                                       uint64_t       stack_offset,
                                       lstf_vm_value *value);

/**
 * Gets the return address saved for the current stack frame.
 */
lstf_vm_status lstf_vm_stack_get_frame_return_address(lstf_vm_stack *stack,
                                                      uint8_t      **return_address_ptr);

/**
 * Gets a value from a position relative to the current stack frame.
 */
lstf_vm_status lstf_vm_stack_frame_get_value(lstf_vm_stack  *stack,
                                             int64_t         fp_offset,
                                             lstf_vm_value  *value);

/**
 * Gets the address of a valid value on the stack, relative to the current
 * stack frame.
 */
lstf_vm_status lstf_vm_stack_frame_get_value_address(lstf_vm_stack  *stack,
                                                     int64_t         fp_offset,
                                                     lstf_vm_value **value_ptr);

lstf_vm_status lstf_vm_stack_frame_get_integer(lstf_vm_stack *stack,
                                               int64_t        fp_offset,
                                               int64_t       *value);

lstf_vm_status lstf_vm_stack_frame_get_double(lstf_vm_stack *stack,
                                              int64_t        fp_offset,
                                              double        *value);

lstf_vm_status lstf_vm_stack_frame_get_boolean(lstf_vm_stack *stack,
                                               int64_t        fp_offset,
                                               bool          *value);

lstf_vm_status lstf_vm_stack_frame_get_string(lstf_vm_stack *stack,
                                              int64_t        fp_offset,
                                              string       **value);

lstf_vm_status lstf_vm_stack_frame_get_code_address(lstf_vm_stack *stack,
                                                    int64_t        fp_offset,
                                                    uint8_t      **value);

lstf_vm_status lstf_vm_stack_frame_get_object(lstf_vm_stack *stack,
                                              int64_t        fp_offset,
                                              json_node    **value);

lstf_vm_status lstf_vm_stack_frame_get_array(lstf_vm_stack *stack,
                                             int64_t        fp_offset,
                                             json_node    **value);

lstf_vm_status lstf_vm_stack_frame_get_pattern(lstf_vm_stack *stack,
                                               int64_t        fp_offset,
                                               json_node    **value);

// --- writing before the last values on the stack

/**
 * Sets the stack value at `stack_offset` to `*value`. Modifies the value
 * pointed at to no longer take ownership of the underlying reference (if there
 * is one).
 */
lstf_vm_status lstf_vm_stack_set_value(lstf_vm_stack *stack,
                                       uint64_t       stack_offset,
                                       lstf_vm_value *value);

/**
 * Sets the stack value at `fp_offset` to `*value`, which is relative to the
 * current stack frame. Modifies the value pointed at to no longer take
 * ownership of the underlying reference (if there is one).
 */
lstf_vm_status lstf_vm_stack_set_frame_value(lstf_vm_stack *stack,
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

lstf_vm_status lstf_vm_stack_pop_closure(lstf_vm_stack    *stack,
                                         lstf_vm_closure **value);

lstf_vm_status lstf_vm_stack_pop_object(lstf_vm_stack *stack,
                                        json_node    **value);

lstf_vm_status lstf_vm_stack_pop_array(lstf_vm_stack *stack,
                                       json_node    **value);

lstf_vm_status lstf_vm_stack_pop_pattern(lstf_vm_stack *stack,
                                         json_node    **value);

// --- pushing a new value onto the stack

/**
 * Pushes an unowned (just popped, copied, or just created) value onto the
 * stack.
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

lstf_vm_status lstf_vm_stack_push_json(lstf_vm_stack *stack,
                                       json_node     *node);

lstf_vm_status lstf_vm_stack_push_closure(lstf_vm_stack   *stack,
                                          lstf_vm_closure *closure);

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
 *
 * `closure` is either NULL or points to the closure information for this
 * function.
 */
lstf_vm_status lstf_vm_stack_setup_frame(lstf_vm_stack   *stack,
                                         uint8_t         *return_address,
                                         lstf_vm_closure *closure);

// --- up-values

/**
 * Gets the n'th up-value for the closure of the current stack frame. Will fail
 * if there is no closure, or if `upvalue_id` is out of range.
 */
lstf_vm_status lstf_vm_stack_frame_get_upvalue(lstf_vm_stack    *stack,
                                               uint8_t           upvalue_id,
                                               lstf_vm_upvalue **upvalue);

/**
 * Assuming `upvalue` is a capture of `fp_offset`, this tracks the up-value to
 * the stack slot so that we know to up-lift it when the current stack frame is
 * popped.
 *
 * `fp_offset` must be > 0
 */
lstf_vm_status lstf_vm_stack_frame_track_upvalue(lstf_vm_stack   *stack,
                                                 int64_t          fp_offset,
                                                 lstf_vm_upvalue *upvalue);

/**
 * Get the up-value that is already tracked in the current frame.
 *
 * `fp_offset` must be > 0
 */
lstf_vm_status lstf_vm_stack_frame_get_tracked_upvalue(lstf_vm_stack    *stack,
                                                       int64_t           fp_offset,
                                                       lstf_vm_upvalue **upvalue);
