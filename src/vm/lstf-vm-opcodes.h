#pragma once
#include "lstf-common.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * This has the `packed` attribute just to make it easier to debug an
 * instruction buffer in GDB by casting it to `lstf_vm_opcode`.
 */
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

    /**
     * `pop` - pops the value at the top of the stack
     */
    lstf_vm_op_pop,

    // --- accessing members of structured types

    /**
     * `get` - read a member/element of an object/array
     */
    lstf_vm_op_get,

    /**
     * `set` - write to a member/element of an object/array
     */
    lstf_vm_op_set,

    /**
     * `append` - append an element to an array
     */
    lstf_vm_op_append,

    /**
     * `in` - test whether an object has a member, or whether an array contains
     *   a value/object
     */
    lstf_vm_op_in,

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
     * `calli` - indirectly calls a function or invokes a closure
     */
    lstf_vm_op_calli,

    /**
     * `schedule <address> <params>` - schedule execution of a function/closure (AKA begin coroutine)
     */
    lstf_vm_op_schedule,

    /**
     * `schedulei <params>` - indirectly begin a coroutine
     */
    lstf_vm_op_schedulei,

    /**
     * `return` - returns from a function
     */
    lstf_vm_op_return,

    /**
     * `closure <n: 0-255> <address> [(is_local_0, index_0) ...]` - creates a
     * closure enclosing `n` values, and pushes this closure on the top of the
     * stack
     *
     * Then the next `n` pairs must be `(is_local, index)`
     * - `is_local` - determines whether we're capturing a local value or an up-value
     * - `index` - either a frame offset or the index of the up-value we're capturing
     */
    lstf_vm_op_closure,

    /**
     * `upget <n: 0-255>` - gets the n'th enclosed variable for the
     *     currently-executing closure
     */
    lstf_vm_op_upget,

    /**
     * `upset <n: 0-255>` - sets the n'th enclosed variable to the topmost
     *     value on the stack, and pops the stack
     */
    lstf_vm_op_upset,

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

    /**
     * `equal` - check for equality between two items, or check that a pattern matches an expression.
     */
    lstf_vm_op_equal,
    lstf_vm_op_greaterthan,
    lstf_vm_op_greaterthan_equal,

    // --- arithmetic operations
    lstf_vm_op_add,
    lstf_vm_op_sub,
    lstf_vm_op_mul,
    lstf_vm_op_div,
    lstf_vm_op_pow,
    lstf_vm_op_mod,
    lstf_vm_op_neg,

    // --- bitwise operations
    lstf_vm_op_and,
    lstf_vm_op_or,
    lstf_vm_op_xor,
    lstf_vm_op_lshift,
    lstf_vm_op_rshift,
    lstf_vm_op_not,

    // --- input/output
    /**
     * `print` - pops the stack and prints the value to standard output
     */
    lstf_vm_op_print,

    /**
     * `exit <integer: 0-255>` exits with the exit code
     */
    lstf_vm_op_exit,

    // --- miscellaneous
    /**
     * Raise an exception if the previous result was not true.
     */
    lstf_vm_op_assert
} __attribute__((packed));
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

static inline const char *lstf_vm_opcode_to_string(lstf_vm_opcode opcode)
{
    switch (opcode) {
        case lstf_vm_op_load_frameoffset:
            return "loadframe";
        case lstf_vm_op_load_dataoffset:
            return "loaddata";
        case lstf_vm_op_load_codeoffset:
            return "loadaddress";
        case lstf_vm_op_load_expression:
            return "loadexpr";
        case lstf_vm_op_store:
            return "store";
        case lstf_vm_op_pop:
            return "pop";
        case lstf_vm_op_get:
            return "get";
        case lstf_vm_op_set:
            return "set";
        case lstf_vm_op_append:
            return "append";
        case lstf_vm_op_in:
            return "in";
        case lstf_vm_op_params:
            return "params";
        case lstf_vm_op_call:
            return "call";
        case lstf_vm_op_calli:
            return "calli";
        case lstf_vm_op_schedule:
            return "schedule";
        case lstf_vm_op_schedulei:
            return "schedulei";
        case lstf_vm_op_return:
            return "return";
        case lstf_vm_op_closure:
            return "closure";
        case lstf_vm_op_upget:
            return "upget";
        case lstf_vm_op_upset:
            return "upset";
        case lstf_vm_op_vmcall:
            return "vmcall";
        case lstf_vm_op_else:
            return "else";
        case lstf_vm_op_jump:
            return "jump";
        case lstf_vm_op_bool:
            return "bool";
        case lstf_vm_op_land:
            return "land";
        case lstf_vm_op_lor:
            return "lor";
        case lstf_vm_op_lnot:
            return "lnot";
        case lstf_vm_op_lessthan:
            return "lessthan";
        case lstf_vm_op_lessthan_equal:
            return "lessthaneq";
        case lstf_vm_op_equal:
            return "eq";
        case lstf_vm_op_greaterthan:
            return "greaterthan";
        case lstf_vm_op_greaterthan_equal:
            return "greatherthaneq";
        case lstf_vm_op_add:
            return "add";
        case lstf_vm_op_sub:
            return "sub";
        case lstf_vm_op_mul:
            return "mul";
        case lstf_vm_op_div:
            return "div";
        case lstf_vm_op_pow:
            return "pow";
        case lstf_vm_op_mod:
            return "mod";
        case lstf_vm_op_neg:
            return "neg";
        case lstf_vm_op_and:
            return "and";
        case lstf_vm_op_or:
            return "or";
        case lstf_vm_op_xor:
            return "xor";
        case lstf_vm_op_lshift:
            return "lshift";
        case lstf_vm_op_rshift:
            return "rshift";
        case lstf_vm_op_not:
            return "not";
        case lstf_vm_op_print:
            return "print";
        case lstf_vm_op_exit:
            return "exit";
        case lstf_vm_op_assert:
            return "assert";
    }

    fprintf(stderr, "%s: unreachable code (unexpected opcode %u)\n",
            __func__, opcode);
    abort();
}
