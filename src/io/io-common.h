#pragma once

/**
 * Returns a new string that is the filename for the file descriptor, or NULL
 * on failure.
 */
char *io_get_filename_from_fd(int fd);

/**
 * Get the process ID
 */
int io_getpid(void);
