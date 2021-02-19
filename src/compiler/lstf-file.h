#pragma once

#include "data-structures/ptr-list.h"
#include <stdbool.h>
#include <limits.h>

typedef struct _lstf_function lstf_function;

struct _lstf_file {
    unsigned refcount : sizeof(unsigned) * CHAR_BIT - 1;
    bool floating : 1;

    /**
     * This is initialized later by the scanner.
     */
    unsigned total_lines;

    /**
     * Absolute path to location of file.
     */
    char *filename;

    /**
     * File content
     */
    char *content;

    lstf_function *main_function;
};
typedef struct _lstf_file lstf_file;

/**
 * Open a new file.
 */
lstf_file *lstf_file_load(const char *filename);

lstf_file *lstf_file_ref(lstf_file *file);

void lstf_file_unref(lstf_file *file);
