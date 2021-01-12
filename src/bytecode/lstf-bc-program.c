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

    if (!program)
        return NULL;

    program->source_filename = source_filename ? strdup(source_filename) : NULL;
    program->debug_sourcemap = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, NULL, free);
    program->debug_symbols = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, NULL, NULL);

    program->comments = ptr_hashmap_new(ptrhash, NULL, NULL, NULL, (collection_item_ref_func) strdup, free);

    // program->data_strings = ptr_hashset_new(strhash, (collection_item_ref_func) strdup, free);
    
    return program;
}

char *lstf_bc_program_add_data(lstf_bc_program *program, char *data_string)
{
    assert(data_string && "cannot add NULL data string to program data!");

    ptr_hashmap_entry *entry = ptr_hashmap_get(program->data_strings, data_string);

    // this is an entry within the data section
    if (entry && entry->key == data_string)
        return data_string;

    // there is already a duplicate entry in the data section
    if (entry) {
        free(data_string);
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
