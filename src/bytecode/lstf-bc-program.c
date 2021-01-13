#include "lstf-bc-program.h"
#include "data-structures/ptr-hashmap.h"
#include "util.h"
#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

lstf_bc_program *lstf_bc_program_new(const char *source_filename)
{
    lstf_bc_program *program = calloc(1, sizeof *program);

    if (!program) {
        fprintf(stderr, "%s: failed to allocate program: %s\n", __func__, strerror(errno));
        abort();
    }

    program->source_filename = source_filename ? strdup(source_filename) : NULL;
    program->debug_sourcemap = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, NULL, (collection_item_unref_func) ptr_hashmap_destroy);
    program->debug_symbols = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, NULL, free);

    program->comments = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, NULL, (collection_item_unref_func) ptr_hashmap_destroy);

    program->data_strings = ptr_hashmap_new((collection_item_hash_func) strhash,
            (collection_item_ref_func) NULL,
            (collection_item_unref_func) NULL,
            (collection_item_equality_func) strequal,
            NULL,
            NULL);

    program->data_bufsize = 1024;
    program->data = calloc(1, program->data_bufsize);

    if (!program->data) {
        fprintf(stderr, "%s: failed to allocate data region: %s\n", __func__, strerror(errno));
        abort();
    }

    program->functions = ptr_hashmap_new((collection_item_hash_func) strhash,
            NULL,
            NULL,
            (collection_item_equality_func) strequal,
            NULL,
            (collection_item_unref_func) lstf_bc_function_destroy);

    program->code_offsets = ptr_hashmap_new((collection_item_hash_func) ptrhash, NULL, NULL, NULL, NULL, free);
    
    return program;
}

void lstf_bc_program_destroy(lstf_bc_program *program)
{
    free(program->source_filename);
    ptr_hashmap_destroy(program->debug_sourcemap);
    ptr_hashmap_destroy(program->debug_symbols);
    ptr_hashmap_destroy(program->comments);
    ptr_hashmap_destroy(program->data_strings);
    free(program->data);
    ptr_hashmap_destroy(program->functions);
    ptr_hashmap_destroy(program->code_offsets);
    memset(program, 0, sizeof *program);
    free(program);
}

void lstf_bc_program_add_sourcemap(lstf_bc_program     *program,
                                   lstf_bc_function    *function,
                                   lstf_bc_instruction *instruction,
                                   uint32_t             line,
                                   uint32_t             column)
{
    assert(program->source_filename && "cannot add sourcemap without source file!");

    const ptr_hashmap_entry *function_entry = ptr_hashmap_get(program->debug_sourcemap, function);
    ptr_hashmap *function_source_map = NULL;

    if (!function_entry) {
        function_source_map = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, NULL, free);
        ptr_hashmap_insert(program->debug_sourcemap, function, function_source_map);
    } else {
        function_source_map = function_entry->value;
    }

    lstf_bc_debugentry *entry = calloc(1, sizeof *entry);

    if (!entry) {
        fprintf(stderr, "%s: failed to create entry: %s\n", __func__, strerror(errno));
        abort();
    }

    entry->line = line;
    entry->column = column;

    ptr_hashmap_insert(function_source_map, instruction, entry);
}

void lstf_bc_program_add_symbol(lstf_bc_program     *program,
                                lstf_bc_function    *function,
                                lstf_bc_instruction *instruction,
                                const char          *symbol_name)
{
    const ptr_hashmap_entry *function_entry = ptr_hashmap_get(program->debug_symbols, function);
    ptr_hashmap *function_symbols = NULL;

    if (!function_entry) {
        function_symbols = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, NULL, free);
        ptr_hashmap_insert(program->debug_symbols, function, function_symbols);
    } else {
        function_symbols = function_entry->value;
    }

    ptr_hashmap_insert(function_symbols, instruction, strdup(symbol_name));
}

void lstf_bc_program_add_comment(lstf_bc_program     *program,
                                 lstf_bc_function    *function,
                                 lstf_bc_instruction *instruction,
                                 const char          *comment)
{
    const ptr_hashmap_entry *function_entry = ptr_hashmap_get(program->comments, function);
    ptr_hashmap *function_comments = NULL;

    if (!function_entry) {
        function_comments = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, NULL, free);
        ptr_hashmap_insert(program->comments, function, function_comments);
    } else {
        function_comments = function_entry->value;
    }


    ptr_hashmap_insert(program->comments, instruction, strdup(comment));
}

char *lstf_bc_program_add_data(lstf_bc_program *program, const char *data_string)
{
    assert(data_string && "cannot add NULL data string to program data!");

    ptr_hashmap_entry *entry = ptr_hashmap_get(program->data_strings, data_string);

    // this is an entry within the data section
    if (entry && entry->key == data_string)
        return entry->key;

    // there is already a duplicate entry in the data section
    if (entry) {
        return entry->key;
    }

    // we have to create an entry
    const size_t string_size = strlen(data_string) + 1;

    // resize data if necessary
    while (program->data_length + string_size > program->data_bufsize) {
        const size_t new_data_bufsize = program->data_bufsize + program->data_bufsize / 4;
        char *new_data = realloc(program->data, new_data_bufsize);

        if (!new_data) {
            fprintf(stderr, "%s: failed to resize program data section: %s\n",
                    __func__, strerror(errno));
            abort();
        }

        program->data = new_data;
        program->data_bufsize = new_data_bufsize;
    }

    char *interned_string = memcpy(program->data + program->data_length, data_string, string_size);
    program->data_length += string_size;

    return interned_string;
}

void lstf_bc_program_add_function(lstf_bc_program  *program,
                                  lstf_bc_function *function)
{
    ptr_hashmap_insert(program->functions, function->name, function);
}
