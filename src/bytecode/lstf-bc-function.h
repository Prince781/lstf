#pragma once

#include "data-structures/ptr-list.h"
#include "bytecode/lstf-bc-instruction.h"
#include <stddef.h>

struct _lstf_bc_function {
    /**
     * The name of the function. Can be NULL.
     */
    char *name;

    /**
     * A flat array of instructions.
     */
    lstf_bc_instruction *instructions;

    unsigned long instructions_length;
    unsigned long instructions_bufsize;
};
typedef struct _lstf_bc_function lstf_bc_function;

/**
 * Creates a new bytecode function.
 *
 * @param name can be NULL
 */
lstf_bc_function *lstf_bc_function_new(const char *name);

void lstf_bc_function_destroy(lstf_bc_function *function);

/**
 * Returns a pointer to the instruction in the new slot.
 */
lstf_bc_instruction *lstf_bc_function_add_instruction(lstf_bc_function   *function,
                                                      lstf_bc_instruction instruction);
