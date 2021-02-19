#pragma once

#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include "io/outputstream.h"
#include "lstf-ir-program.h"
#include "lstf-codevisitor.h"
#include "lstf-file.h"
#include <limits.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

struct _lstf_codegenerator {
    lstf_codevisitor parent_struct;
    unsigned refcount : sizeof(unsigned) * CHAR_BIT - 1;
    bool floating : 1;
    unsigned num_errors;

    lstf_file *file;

    /**
     * Intermediate representation
     */
    lstf_ir_program *ir;

    /**
     * maps `(lstf_scope *) -> (lstf_ir_basicblock *)`
     */
    ptr_hashmap *scopes_to_basicblocks;

    /**
     * maps `(lstf_scope *) -> (lstf_symbol *) -> (lstf_ir_instruction *)`
     */
    ptr_hashmap *scopes_to_symbols_to_temps;

    /**
     * maps `(lstf_expression *) -> (lstf_ir_instruction *)`
     */
    ptr_hashmap *exprs_to_temps;

    /**
     * list of `(lstf_ir_function *)`
     *
     * Stack of current IR functions.
     */
    ptr_list *ir_functions;

    /**
     * maps `(lstf_codenode *) -> (lstf_ir_function *)`
     */
    ptr_hashmap *codenodes_to_ir_functions;

    /**
     * maps `(lstf_symbol *) -> (lstf_ir_instruction *)`
     *
     * Maps local variables / closures to their allocation instructions.
     */
    ptr_hashmap *local_allocs;

    /**
     * Bytecode output
     */
    outputstream *output;
};
typedef struct _lstf_codegenerator lstf_codegenerator;

/**
 * Converts the syntax tree into executable code. It's not guaranteed that this
 * will check for well-formedness, only create this if there are no errors in
 * semantic analysis.
 */
lstf_codegenerator *lstf_codegenerator_new(lstf_file *file)
    __attribute__((nonnull (1)));

lstf_codegenerator *lstf_codegenerator_ref(lstf_codegenerator *generator);

void lstf_codegenerator_unref(lstf_codegenerator *generator);

/**
 * Compiles source code to bytecode.
 *
 * The steps are:
 * 1. compile to IR and check IR
 *  - compile syntax tree to IR
 *  - run a pass checking for and eliminating dead code
 *  - run a pass computing frame offsets for instructions with a result
 * 2. convert IR to VM instructions
 * 3. serialize VM instructions to bytecode
 *
 * After calling this function, if the code generator encountered no errors,
 * call `lstf_codegenerator_get_compiled_bytecode()` and then call
 * `lstf_vm_loader_load_from_buffer()` to load the program. Stage 1 can fail if
 * there are errors found by the passes, such as dead code or logic errors.
 */
void lstf_codegenerator_compile(lstf_codegenerator *generator);

/**
 * Gets the compiled bytecode. Returns `NULL` if compilation did not complete.
 *
 * @param buffer_size where to place the buffer size. must be non-NULL
 *
 * @return the bytecode buffer, or `NULL` if compilation failed
 */
const uint8_t *lstf_codegenerator_get_compiled_bytecode(const lstf_codegenerator *generator,
                                                        size_t                   *buffer_size);

// TODO: lstf_codegenerator_write_to_file(const lstf_codegenerator *generator, const char *path)
