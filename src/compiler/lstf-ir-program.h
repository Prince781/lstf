#pragma once

#include "bytecode/lstf-bc-program.h"
#include "data-structures/ptr-list.h"
#include "lstf-ir-node.h"
#include "lstf-ir-function.h"
#include <stdbool.h>
#include <limits.h>

struct _lstf_ir_program {
    unsigned long refcount : sizeof(unsigned long)*CHAR_BIT - 1;
    bool floating : 1;

    /**
     * Contains a list of `(lstf_ir_function *)`
     */
    ptr_list *functions;
};
typedef struct _lstf_ir_program lstf_ir_program;

lstf_ir_program *lstf_ir_program_new(void);

lstf_ir_program *lstf_ir_program_ref(lstf_ir_program *program);

void lstf_ir_program_unref(lstf_ir_program *program);

void lstf_ir_program_add_function(lstf_ir_program  *program,
                                  lstf_ir_function *function);

unsigned lstf_ir_program_analyze(lstf_ir_program *program);

/**
 * Assemble the IR into abstract bytecode. To serialize the bytecode, see
 * `lstf_bc_program_serialize_to_binary()`.
 */
lstf_bc_program *lstf_ir_program_assemble(lstf_ir_program *program);

/**
 * Outputs a Graphviz file.
 *
 * Returns whether the operation was successful.
 */
bool lstf_ir_program_visualize(const lstf_ir_program *program, const char *path);
