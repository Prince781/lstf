#pragma once

enum _lstf_vm_status {
    /**
     * No error, and the VM should continue.
     */
    lstf_vm_status_continue,

    /**
     * The `exit` instruction was executed, and the VM should stop.
     */
    lstf_vm_status_exited,
    
    /**
     * A translated frame offset was invalid, past the end or before the start
     * of the value stack.
     */
    lstf_vm_status_invalid_stack_offset,

    /**
     * The requested operand was not of the requested type.
     */
    lstf_vm_status_invalid_operand_type,
    
    /**
     * Invalid/unsupported instruction.
     */
    lstf_vm_status_invalid_instruction,

    /**
     * Attempt to jump to an offset in the code that is out-of-bounds.
     */
    lstf_vm_status_invalid_code_offset,

    /**
     * The `vmcall` instruction was executed with an invalid op code.
     */
    lstf_vm_status_invalid_vmcall,

    /**
     * The `get` instruction attempted to access a member that does not exist
     * on a JSON object, or attempted to access a member on a pattern JSON
     * object.
     */
    lstf_vm_status_invalid_member_access,

    /**
     * The `set` or `get` instruction attempted to access an array with an
     * index that was out-of-bounds.
     */
    lstf_vm_status_index_out_of_bounds,

    /**
     * The `load` instruction for immediate expressions failed to load the
     * expression because it could not be parsed.
     */
    lstf_vm_status_invalid_expression,

    /**
     * Attempt to load an entry in the `data` section that is out-of-bounds.
     */
    lstf_vm_status_invalid_data_offset,

    /**
     * A stack pop operation would have popped a value before the start of the
     * current stack frame. The current stack frame needs to be torn down
     * before this operation is attempted again.
     */
    lstf_vm_status_frame_underflow,

    /**
     * An attempt to tear down a frame failed because there was no frame to
     * tear down.
     */
    lstf_vm_status_invalid_return,

    /**
     * Either the value stack or the frame pointer stack is full.
     */
    lstf_vm_status_stack_overflow,

    /**
     * There was an attempt to push a value onto the stack without having first
     * set up a stack frame.
     */
    lstf_vm_status_invalid_push,
};
typedef enum _lstf_vm_status lstf_vm_status;
