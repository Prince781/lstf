#include "io/io-common.h"
#include "data-structures/string-builder.h"
#include "io/inputstream.h"
#include "io/outputstream.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <threads.h>

#if defined(_WIN32) || defined(_WIN64)
/* WINDOWS */
#include <windows.h>
#include <consoleapi.h>
#include <io.h>
char *io_get_filename_from_fd(int fd) {
    HANDLE fh = (HANDLE) _get_osfhandle(fd);
    char resolved_path[MAX_PATH] = { '\0' };
    DWORD ret = 0;

    if (!fh) {
        fprintf(stderr, "%s: could not get OS f-handle from fd %d\n", __func__, fd);
        return NULL;
    }

    if (!(ret = GetFinalPathNameByHandle(fh, resolved_path, sizeof resolved_path, 0)))
        return NULL;

    return _strdup(resolved_path);
}

int io_getpid(void) {
    return GetCurrentProcessId();
}

bool is_ascii_terminal(FILE *file) {
    HANDLE file_handle = (HANDLE) _get_osfhandle(fileno(file));
    if (file_handle)
        return !!SetConsoleMode(file_handle, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    return false;
}

bool io_communicate(const char    *path,
                    const char   **args,
                    outputstream **in_stream,
                    inputstream  **out_stream,
                    inputstream  **err_stream)
{
    // TODO
}

static mtx_t io_get_current_dir__mutex;

static void io_get_current_dir__mutex_init(void)
{
    if (mtx_init(&io_get_current_dir__mutex, mtx_plain) == thrd_error) {
        fprintf(stderr, "error: failed to initialize mutex for io_get_current_dir(): %s\n",
                strerror(errno));
        abort();
    }
}

const char *io_get_current_dir(void)
{
    static thread_local char buffer[8192 /* should be good enough */];
    static once_flag flag = ONCE_FLAG_INIT;
    call_once(&flag, io_get_current_dir__mutex_init);

    // GetCurrentDirectory() is not thread-safe, so use mutexes
    if (mtx_lock(&io_get_current_dir__mutex) == thrd_failure) {
        fprintf(stderr, "%s: failed to acquire mutex: %s\n", __func__, strerror(errno));
        abort();
    }
    DWORD result = GetCurrentDirectory(sizeof(buffer) - 1, buffer);
    if (mtx_unlock(&io_get_current_dir__mutex) == thrd_failure) {
        fprintf(stderr, "%s: failed to release mutex: %s\n", __func__, strerror(errno));
        abort();
    }
    assert((result && result <= sizeof buffer) && "path may be too long! increase buffer!");
    return buffer;
}
#else
/* UNIX */
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>
char *io_get_filename_from_fd(int fd)
{
    string *path_sb = string_new();
    char resolved_path[PATH_MAX] = { '\0' };
    ssize_t ret = 0;

    string_appendf(path_sb, "/proc/self/fd/%d", fd); 
    ret = readlink(path_sb->buffer, resolved_path, sizeof resolved_path);
    free(string_destroy(path_sb));

    if (ret != -1)
        return strdup(resolved_path);
    return NULL;
}

int io_getpid(void) {
    return getpid();
}

bool is_ascii_terminal(FILE *file) {
    return isatty(fileno(file));
}

bool io_communicate(const char    *path,
                    const char   **args,
                    outputstream **in_stream,
                    inputstream  **out_stream,
                    inputstream  **err_stream)
{
    assert((in_stream || out_stream || err_stream) && "at least one stream must be specified");
    int saved_errno = 0;

    // create two pipes
    int stdin_pipe_fds[2] = {-1, -1};
    int stdout_pipe_fds[2] = {-1, -1};
    int stderr_pipe_fds[2] = {-1, -1};
    pid_t child_pid = -1;
    if (in_stream && pipe(stdin_pipe_fds) != 0)
        goto cleanup_on_error;
    if (out_stream && pipe(stdout_pipe_fds) != 0)
        goto cleanup_on_error;
    if (err_stream && pipe(stderr_pipe_fds) != 0)
        goto cleanup_on_error;

    // fork
    if ((child_pid = fork()) == -1) {
        goto cleanup_on_error;
    } else if (child_pid == 0) {
        // --- child ---
        // connect stdin/stdout/stderr
        if (in_stream && dup2(stdin_pipe_fds[0], STDIN_FILENO) == -1) {
            fprintf(stderr, "error: could not dup read end of stdin pipe: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (out_stream && dup2(stdout_pipe_fds[1], STDOUT_FILENO) == -1) {
            fprintf(stderr, "error: could not dup write end of stdout pipe: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (err_stream && dup2(stderr_pipe_fds[1], STDERR_FILENO) == -1) {
            fprintf(stderr, "error: could not dup write end of stderr pipe: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // copy the arguments
        size_t argv_l = 4,
               argv_i = 0;
        char **argv = calloc(argv_l, sizeof(argv[0]));
        for (argv_i = 0; args[argv_i]; argv_i++) {
            if (argv_i + 1 >= argv_l) {
                // resize
                argv_l *= 2;
                char **argv_new;
                if (!(argv_new = realloc(argv, argv_l * sizeof(argv[0])))) {
                    fprintf(stderr, "error: could not copy arguments for exec(): %s\n", strerror(errno));
                    exit(EXIT_FAILURE);
                }
                argv = argv_new;
            }
            if (!(argv[argv_i] = strdup(args[argv_i]))) {
                fprintf(stderr, "error: could not copy argument #%zu for exec(): %s\n", argv_i, strerror(errno));
                fprintf(stderr, "note: argument #%zu is: %s\n", argv_i, args[argv_i]);
                exit(EXIT_FAILURE);
            }
        }
        argv[argv_i] = NULL;
        if (execvp(path, argv) != 0) {
            fprintf(stderr, "error: could not launch process `%s': %s\n", path, strerror(errno));
            exit(EXIT_FAILURE);
        }
    } else {
        // --- parent ---
        // setup streams
        if (in_stream && !(*in_stream = outputstream_new_from_fd(stdin_pipe_fds[1], true)))
            goto cleanup_on_error;
        if (out_stream && !(*out_stream = inputstream_new_from_fd(stdout_pipe_fds[0], true)))
            goto cleanup_on_error;
        if (err_stream && !(*err_stream = inputstream_new_from_fd(stderr_pipe_fds[0], true)))
            goto cleanup_on_error;
        return true;
    }

cleanup_on_error:
    saved_errno = errno;
    close(stdin_pipe_fds[0]);
    close(stdin_pipe_fds[1]);
    close(stdout_pipe_fds[0]);
    close(stdout_pipe_fds[1]);
    close(stderr_pipe_fds[0]);
    close(stderr_pipe_fds[1]);
    if (child_pid != -1) {
        // end child process
        if (kill(child_pid, SIGTERM) == -1) {
            fprintf(stderr, "warning: failed to signal process %d: %s\n",
                    child_pid, strerror(errno));
        } else {
            // wait for termination
            int status = 0;
            for (unsigned try = 0; try < 3; try++) {
                if (waitpid(child_pid, &status, WNOHANG) == -1) {
                    fprintf(stderr, "warning: failed to wait for child %d to terminate: %s\n",
                            child_pid, strerror(errno));
                    if (errno == ECHILD)
                        break;
                } else if (WIFEXITED(status)) {
                    break;
                } else if (try < 1) {
                    // give 5 seconds before killing
                    sleep(5);
                } else {
                    if (kill(child_pid, SIGKILL) == -1) {
                        fprintf(stderr, "warning: failed to kill process %d: %s\n",
                                child_pid, strerror(errno));
                        break;
                    }
                    // reap process on next iteration
                }
            }
        }
    }
    errno = saved_errno;
    return false;
}

const char *io_get_current_dir(void)
{
    static thread_local char buffer[8192 /* should be good enough */];
    assert(getcwd(buffer, sizeof(buffer)-1) && "path is too long! increase buffer size!");
    return buffer;
}
#endif
