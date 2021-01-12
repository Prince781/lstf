#pragma once

#include "data-structures/ptr-list.h"
#include "bytecode/lstf-bc-instruction.h"
#include <stddef.h>

struct _lstf_bc_function {
    /**
     * The name of the function.
     */
    char *name;

    /**
     * A flat array of instructions.
     */
    lstf_bc_instruction *instructions;

    size_t instructions_length;
    size_t instructions_bufsize;
};
typedef struct _lstf_bc_function lstf_bc_function;

lstf_bc_function *lstf_bc_function_new(const char *name);

void lstf_bc_function_destroy(lstf_bc_function *function);

/**
 * Returns a pointer to the instruction in the new slot.
 */
lstf_bc_instruction *lstf_bc_function_add_instruction(lstf_bc_function   *function,
                                                      lstf_bc_instruction instruction);
