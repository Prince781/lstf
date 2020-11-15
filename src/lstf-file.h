#pragma once
#include <stdio.h>

struct _lstf_file {
    /**
     * Absolute path to location of file.
     */
    char *filename;

    /**
     * File content
     */
    char *content;
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
