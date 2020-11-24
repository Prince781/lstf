#pragma once

#include "data-structures/ptr-list.h"

struct _lstf_block;
typedef struct _lstf_block lstf_block;

struct _lstf_file {
    /**
     * Absolute path to location of file.
     */
    char *filename;

    /**
     * File content
     */
    char *content;

    lstf_block *main_block;
};
typedef struct _lstf_file lstf_file;

/**
 * Open a new file.
 */
lstf_file *lstf_file_load(const char *filename);

/**
 * Destroy LSTF script
 */
void lstf_file_unload(lstf_file *file);
