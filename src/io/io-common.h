#pragma once

#include "lstf-common.h"
#include <stdio.h>
#include <stdbool.h>

typedef struct _inputstream inputstream;
typedef struct _outputstream outputstream;

/**
 * Returns a new string that is the filename for the file descriptor, or NULL
 * on failure.
 */
char *io_get_filename_from_fd(int fd);

/**
 * Get the process ID
 */
int io_getpid(void);

/**
 * Determines whether the file is actually a handle to a terminal device.
 */
bool is_ascii_terminal(FILE *file);

/**
 * Launches a new process for communicating.
 *
 * @param path          path of the program, absolute or relative
 * @param args          arguments to pass, or NULL
 * @param in_stream     the child's stdin to write to
 * @param out_stream    the child's stdout to read from
 * @param err_stream    the child's stderr to read from
 *
 * @return true on success. false on error. caller is responsible for free()'ing resources if non-NULL
 */
bool io_communicate(const char    *path,
                    const char   **args,
                    outputstream **in_stream,
                    inputstream  **out_stream,
                    inputstream  **err_stream)
    __attribute__((nonnull (1, 2)));

/**
 * Gets the current working directory
 */
const char *io_get_current_dir(void);
