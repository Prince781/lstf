#pragma once

#include <stdbool.h>
#include <sys/types.h>

typedef struct _inputstream inputstream;
typedef struct _outputstream outputstream;

/**
 * Get the process ID
 */
int io_getpid(void);

#if defined(_WIN32) || defined(_WIN64)
/**
 * A handle to a process.
 */
typedef HANDLE io_process;
#else
/**
 * A process ID.
 */
typedef pid_t io_process;

/**
 * A process and status pair.
 */
typedef struct {
    io_process process;
    int        status;
} io_procstat;
#endif

/**
 * Launches a new process for communicating.
 *
 * @param path          path of the program, absolute or relative
 * @param args          arguments to pass, or NULL
 * @param in_stream     (optional) the child's stdin to write to
 * @param out_stream    (optional) the child's stdout to read from
 * @param err_stream    (optional) the child's stderr to read from
 * @param process       (optional) a handle or ID of the subprocess
 *
 * @return true on success. false on error. caller is responsible for free()'ing
 *         resources if non-NULL
 */
bool io_communicate(const char    *path,
                    const char   **args,
                    outputstream **in_stream,
                    inputstream  **out_stream,
                    inputstream  **err_stream,
                    io_process    *subprocess)
    __attribute__((nonnull(1, 2), warn_unused_result));
