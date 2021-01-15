#pragma once

enum _lstf_vm_opcode {
    // --- reading/writing to and from memory

    /**
     * `load frame(<n>)` - loads the n'th item in the current stack frame.
     */
    lstf_vm_op_load_frameoffset = 0x01,

    /**
     * `load data(<n>)` - loads the n'th item in the data section
     */
    lstf_vm_op_load_dataoffset,

    /**
     * `load <function>` - loads an immediate 8-byte code offset
     */
    lstf_vm_op_load_codeoffset,

    /**
     * parses a JSON immediate after the current instruction
     */ 
    lstf_vm_op_load_expression,

    /**
     * `store frame(<n>)` - pops the value at the top of the stack and stores
     *     it into the n'th item on the stack frame
     */
    lstf_vm_op_store,

    // --- accessing members of structured types

    /**
     * `get` - read a member/element of an object/array
     */
    lstf_vm_op_get,

    /**
     * `set` - write to a member/element of an object/array
     */
    lstf_vm_op_set,

    // --- calling/returning from functions

    /**
     * `params <integer: 0-255>` - loads N params from the parent stack frame,
     *     which will later be popped by the `return` operation
     */
    lstf_vm_op_params,

    /**
     * `call <address>` - calls a function
     */
    lstf_vm_op_call,

    /**
     * `indirect` - indirectly calls a function
     */
    lstf_vm_op_indirect,

    /**
     * `return` - returns from a function
     */
    lstf_vm_op_return,

    /**
     * `vmcall <opcode>` - calls a special function provided by the virtual
     *     machine. See `lstf_vm_vmcall_op`.
     */
    lstf_vm_op_vmcall,

    // --- control flow

    /**
     * `else <address>` - jumps to the address if the previous expression
     *     evaluated to `false`
     */
    lstf_vm_op_else,

    /**
     * `jump <address>` - jumps to the address unconditionally
     */
    lstf_vm_op_jump,

    // --- logical operations
    lstf_vm_op_bool,
    lstf_vm_op_land,
    lstf_vm_op_lor,
    lstf_vm_op_lnot,

    // --- comparison operations
    lstf_vm_op_lessthan,
    lstf_vm_op_lessthan_equal,
    lstf_vm_op_equal,
    lstf_vm_op_greaterthan,
    lstf_vm_op_greaterthan_equal,

    // --- arithmetic operations
    lstf_vm_op_add,
    lstf_vm_op_sub,
    lstf_vm_op_mul,
    lstf_vm_op_div,

    // --- input/output
    /**
     * `print` - pops the stack and prints the value to standard output
     */
    lstf_vm_op_print,

    /**
     * `exit <integer: 0-255>` exits with the exit code
     */
    lstf_vm_op_exit
};
typedef enum _lstf_vm_opcode lstf_vm_opcode;

enum _lstf_vm_vmcallcode {
    /**
     * `function connect(path_to_server: string): void`
     *
     * Connect to LSP server. On failure throws a fatal exception.
     */
    lstf_vm_vmcall_connect,

    /**
     * `function td_open(filename: string): void`
     *
     * Call `textDocument/open` with a fail. Will fail if a connection has not
     * yet been established.
     */
    lstf_vm_vmcall_td_open,

    /**
     * `async function diagnostics(filename: string): PublishDiagnosticsParams`
     */
    lstf_vm_vmcall_diagnostics
};
typedef enum _lstf_vm_vmcallcode lstf_vm_vmcallcode;
