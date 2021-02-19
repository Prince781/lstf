#pragma once

#include "lstf-ir-node.h"
#include "vm/lstf-vm-opcodes.h"
#include "data-structures/ptr-hashset.h"
#include "lstf-ir-instruction.h"
#include "lstf-ir-basicblock.h"
#include <stdbool.h>
#include <stdint.h>

struct _lstf_ir_function {
    lstf_ir_node parent_struct;

    /**
     * Number of parameters this function is called with. The VM instruction
     * used for loading parameters is limited to 255.
     */
    uint8_t parameters;

    /**
     * Number of up-values this function holds a reference to. This is greater
     * than 0 if this function is a closure.
     */
    uint8_t upvalues;

    /**
     * Whether this function returns a result
     */
    bool has_result : 1;

    /**
     * Whether this function returns
     */
    bool does_return : 1;

    /**
     * Whether this function is not created from user code, but is instead a
     * function or instruction that's supported by the VM.
     */
    bool is_vm_defined : 1;

    /**
     * The number of locals ever declared in any block within this function.
     */
    unsigned num_locals;

    /**
     * The instruction op code for this built-in function.
     */
    lstf_vm_opcode vm_opcode;

    /**
     * Only applicable if `is_vm_defined == true` and `vm_opcode` is
     * `lstf_vm_op_vmcall`
     */
    lstf_vm_vmcallcode vm_callcode;

    /**
     * A special name for the function, used for debugging.
     *
     * Is NULL if `is_vm_defined` is true.
     */
    char *name;

    /**
     * The basic block that the function always begins in.
     */
    lstf_ir_basicblock *entry_block;

    /**
     * The basic block that leads out of the function.
     */
    lstf_ir_basicblock *exit_block;

    /**
     * This is a hash set of `(lstf_ir_basicblock *)` elements. This member
     * will be NULL if `is_vm_defined == true`
     */
    ptr_hashset *basic_blocks;
};
typedef struct _lstf_ir_function lstf_ir_function;

/**
 * Create a new IR function for user-defined code.
 */
lstf_ir_function *lstf_ir_function_new_for_userfn(const char *name,
                                                  uint8_t     parameters,
                                                  uint8_t     num_upvalues,
                                                  bool        has_result);

/**
 * Create a new IR function for a VM-supported instruction or function.
 */
lstf_ir_function *lstf_ir_function_new_for_instruction(const char        *name,
                                                       uint8_t            parameters,
                                                       bool               has_result,
                                                       bool               does_return,
                                                       lstf_vm_opcode     vm_opcode,
                                                       lstf_vm_vmcallcode vm_callcode);

/**
 * Adds a new basic block to the function.
 */
void lstf_ir_function_add_basic_block(lstf_ir_function *fn, lstf_ir_basicblock *block);
