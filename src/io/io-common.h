#pragma once

#include <stdio.h>
#include <stdbool.h>

/**
 * Returns a new string that is the filename for the file descriptor, or NULL
 * on failure.
 */
char *io_get_filename_from_fd(int fd);

/**
 * Determines whether the file is actually a handle to a terminal device.
 */
bool is_ascii_terminal(FILE *file);

/**
 * Gets the current working directory
 */
const char *io_get_current_dir(void);
