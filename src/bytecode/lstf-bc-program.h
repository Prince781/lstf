#pragma once

#include "data-structures/ptr-hashmap.h"
#include "lstf-bc-function.h"
#include "lstf-bc-instruction.h"
#include <stdint.h>

struct _lstf_bc_debugentry {
    uint32_t line;
    uint32_t column;
};
typedef struct _lstf_bc_debugentry lstf_bc_debugentry;

struct _lstf_bc_program {
    // --- debug info

    /**
     * The path to the source file
     */
    char *source_filename;

    /**
     * Maps `(function: lstf_bc_function *) -> ((instruction: lstf_bc_instruction *) -> (entry: lstf_bc_debugentry *))`
     */
    ptr_hashmap *debug_sourcemap;

    /**
     * Maps `(function: lstf_bc_function *) -> ((instruction: lstf_bc_instruction *) -> (name: char *))`
     */
    ptr_hashmap *debug_symbols;

    // --- comments

    /**
     * Maps `(function: lstf_bc_function *) -> ((instruction: lstf_bc_instruction *) -> (name: char *))`
     */
    ptr_hashmap *comments;

    // --- data

    /**
     * A hashset of `(char *) -> (char *)`
     *
     * The pointers in this map point within the contiguous data section.
     */
    ptr_hashmap *data_strings;

    /**
     * The contiguous data section, filled with `data_length` bytes.
     */
    char *data;
    size_t data_length;
    size_t data_bufsize;

    // --- code

    /**
     * Maps `(name: char *) -> (function: lstf_bc_function *)`
     */
    ptr_hashmap *functions;

    /**
     * Maps `(function: lstf_bc_function *) -> (uint64_t[])`
     */
    ptr_hashmap *code_offsets;
};
typedef struct _lstf_bc_program lstf_bc_program;

/**
 * Creates a new program bytecode.
 *
 * @param source_filename can be NULL
 */
lstf_bc_program *lstf_bc_program_new(const char *source_filename);

void lstf_bc_program_destroy(lstf_bc_program *program);

// --- debug

/**
 * Associates the `instruction` with the line and column of `source_filename`.
 */
void lstf_bc_program_add_sourcemap(lstf_bc_program     *program,
                                   lstf_bc_function    *function,
                                   lstf_bc_instruction *instruction,
                                   uint32_t             line,
                                   uint32_t             column);

/**
 * Associates the `instruction` with the symbol name.
 */
void lstf_bc_program_add_symbol(lstf_bc_program     *program,
                                lstf_bc_function    *function,
                                lstf_bc_instruction *instruction,
                                const char          *symbol_name);

// --- comments

/**
 * Adds a comment to the program's `comments` section, associated with the
 * `instruction`.
 */
void lstf_bc_program_add_comment(lstf_bc_program     *program,
                                 lstf_bc_function    *function,
                                 lstf_bc_instruction *instruction,
                                 const char          *comment);

// --- data

/**
 * Adds `data_string` to the program's data.
 *
 * @param data_string the buffer to add to the program's data section
 *
 * @return relative distance to the start of the program's data section
 *
 * @see lstf_bc_instruction_load_dataoffset_new
 */
uint64_t lstf_bc_program_add_data(lstf_bc_program *program, const char *data_string);

// --- code

/**
 * Adds a new function to the program bytecode.
 */
void lstf_bc_program_add_function(lstf_bc_program  *program,
                                  lstf_bc_function *function);
