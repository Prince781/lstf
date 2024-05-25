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
// #include <fcntl.h> // for flags

char *io_get_filename_from_fd(int fd) {
    HANDLE fh = (HANDLE) _get_osfhandle(fd);
    char resolved_path[MAX_PATH] = {};
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
    assert((in_stream || out_stream || err_stream) && "at least one stream must be specified");

    int saved_errno = 0;

    // create three pipes
    struct pipe_info {
        HANDLE read_handle;
        HANDLE write_handle;
    };
    struct pipe_info child_stdin = {};
    struct pipe_info child_stdout = {};
    struct pipe_info child_stderr = {};

    SECURITY_ATTRIBUTES security_attrs = {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .bInheritHandle = true
    };

    string *command_line = NULL;

    int child_stdin_write_fd = -1,
        child_stdout_read_fd = -1,
        child_stderr_read_fd = -1;

    // setup the pipe handles

    if (in_stream && !CreatePipe(&child_stdin.read_handle, &child_stdin.write_handle,
                    &security_attrs, 0))
        goto cleanup_on_error;

    if (in_stream && !SetHandleInformation(child_stdin.write_handle, HANDLE_FLAG_INHERIT, 0))
        goto cleanup_on_error;

    if (out_stream && !CreatePipe(&child_stdout.read_handle, &child_stdout.write_handle,
                    &security_attrs, 0))
        goto cleanup_on_error;

    if (out_stream && !SetHandleInformation(child_stdout.read_handle, HANDLE_FLAG_INHERIT, 0))
        goto cleanup_on_error;

    if (err_stream && !CreatePipe(&child_stderr.read_handle, &child_stderr.write_handle,
                    &security_attrs, 0))
        goto cleanup_on_error;

    if (err_stream && !SetHandleInformation(child_stderr.read_handle, HANDLE_FLAG_INHERIT, 0))
        goto cleanup_on_error;

    // launch the new process
    PROCESS_INFORMATION process_info = {};

    STARTUPINFO startup_info = {
        .cb = sizeof(STARTUPINFO),
        .hStdInput = child_stdin.read_handle,
        .hStdOutput = child_stdout.write_handle,
        .hStdError = child_stderr.write_handle,
        .dwFlags = STARTF_USESTDHANDLES
    };

    // quote args (skip "path") as it's repeated in args[0]
    command_line = string_new();
    for (const char **argp = args; *argp; argp++) {
        if (argp != args)
            string_appendf(command_line, " ");

        // quote the argument if it needs it
        if (**argp != '"') {
            string_appendf(command_line, "\"");
            for (const char *p = *argp; *p; p++) {
                // escape special characters in quoted argument
                switch (*p) {
                    case '"':
                        string_appendf(command_line, "\\\"");
                        break;
                    case '\\':
                        string_appendf(command_line, "\\\\");
                        break;
                    default:
                        string_appendf(command_line, "%c", *p);
                        break;
                }
            }
            string_appendf(command_line, "\"");
        } else {
            string_appendf(command_line, "%s", *argp);
        }
    }

    // XXX: verify or warn here?
    // see CreateProcess() docs: 
    //  https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa
    // if (command_line->length > 32767) {
    //     errno = ENAMETOOLONG;
    //     goto cleanup_on_error;
    // }

    // launch the process!
    if (!CreateProcess(NULL, command_line->buffer, NULL, NULL, true, 0, NULL, NULL,
                &startup_info, &process_info))
        goto cleanup_on_error;

    // now do some cleanup
    string_unref(command_line);
    command_line = NULL;

    // close handles to the process and thread that we don't need
    CloseHandle(process_info.hProcess);
    process_info.hProcess = NULL;
    CloseHandle(process_info.hThread);
    process_info.hThread = NULL;

    // close the ends of the pipes we opened for the child
    if (child_stdin.read_handle) {
        CloseHandle(child_stdin.read_handle);
        child_stdin.read_handle = NULL;
        startup_info.hStdInput = NULL;
    }
    if (child_stdout.write_handle) {
        CloseHandle(child_stdout.write_handle);
        child_stdout.write_handle = NULL;
        startup_info.hStdOutput = NULL;
    }
    if (child_stderr.write_handle) {
        CloseHandle(child_stdout.write_handle);
        child_stderr.write_handle = NULL;
        startup_info.hStdError = NULL;
    }

    // first, open file descriptors for each of the handles
    if (in_stream && 
            (child_stdin_write_fd = 
                _open_osfhandle((intptr_t) child_stdin.write_handle, 0)) == -1)
        goto cleanup_on_error;
    child_stdin.write_handle = NULL;    // hereafter, use child_stdin_write_fd
    if (out_stream &&
            (child_stdout_read_fd = 
                 _open_osfhandle((intptr_t) child_stdout.read_handle, 0)) == -1)
        goto cleanup_on_error;
    child_stdout.read_handle = NULL;    // hereafter, use child_stdout_read_fd
    if (err_stream &&
            (child_stderr_read_fd =
                 _open_osfhandle((intptr_t) child_stderr.read_handle, 0)) == -1)
        goto cleanup_on_error;
    child_stderr.read_handle = NULL;    // hereafter, use child_stderr_read_fd

    // now wrap each in a stream
    if (in_stream && 
            !(*in_stream = outputstream_new_from_fd(child_stdin_write_fd, true)))
        goto cleanup_on_error;
    if (out_stream &&
            !(*out_stream = inputstream_new_from_fd(child_stdout_read_fd, true)))
        goto cleanup_on_error;
    if (err_stream &&
            !(*err_stream = inputstream_new_from_fd(child_stderr_read_fd, true)))
        goto cleanup_on_error;

    return true;

cleanup_on_error:
    saved_errno = errno;
    if (child_stdin.read_handle)
        CloseHandle(child_stdin.read_handle);
    if (child_stdin.write_handle)
        CloseHandle(child_stdin.write_handle);
    if (child_stdout.read_handle)
        CloseHandle(child_stdout.read_handle);
    if (child_stdout.write_handle)
        CloseHandle(child_stdout.write_handle);
    if (child_stderr.read_handle)
        CloseHandle(child_stdout.read_handle);
    if (child_stderr.write_handle)
        CloseHandle(child_stdout.write_handle);
    string_unref(command_line);
    if (child_stdin_write_fd != -1)
        close(child_stdin_write_fd);
    if (child_stdout_read_fd != -1)
        close(child_stdout_read_fd);
    if (child_stderr_read_fd != -1)
        close(child_stderr_read_fd);
    errno = saved_errno;
    return false;
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
    static thread_local char buffer[MAX_PATH];
    static once_flag flag = ONCE_FLAG_INIT;
    call_once(&flag, io_get_current_dir__mutex_init);

    // GetCurrentDirectory() is not thread-safe, so use mutexes
    if (mtx_lock(&io_get_current_dir__mutex) == thrd_error) {
        fprintf(stderr, "%s: failed to acquire mutex: %s\n", __func__, strerror(errno));
        abort();
    }

    DWORD result = GetCurrentDirectory(sizeof(buffer) - 1, buffer);

    if (mtx_unlock(&io_get_current_dir__mutex) == thrd_error) {
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

    // create three pipes
    union pipe_info {
        struct {
          int read_fd;
          int write_fd;
        };
        int fds[2];
    };
    union pipe_info stdin_pipe = {.fds = {-1, -1}};
    union pipe_info stdout_pipe = {.fds = {-1, -1}};
    union pipe_info stderr_pipe = {.fds = {-1, -1}};
    pid_t child_pid = -1;
    if (in_stream && pipe(stdin_pipe.fds) != 0)
        goto cleanup_on_error;
    if (out_stream && pipe(stdout_pipe.fds) != 0)
        goto cleanup_on_error;
    if (err_stream && pipe(stderr_pipe.fds) != 0)
        goto cleanup_on_error;

    // fork
    if ((child_pid = fork()) == -1) {
        goto cleanup_on_error;
    } else if (child_pid == 0) {
        // --- child ---
        // connect stdin/stdout/stderr
        if (in_stream && dup2(stdin_pipe.read_fd, STDIN_FILENO) == -1) {
            fprintf(stderr, "error: could not dup read end of stdin pipe: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (out_stream && dup2(stdout_pipe.write_fd, STDOUT_FILENO) == -1) {
            fprintf(stderr, "error: could not dup write end of stdout pipe: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (err_stream && dup2(stderr_pipe.write_fd, STDERR_FILENO) == -1) {
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
        if (in_stream && !(*in_stream = outputstream_new_from_fd(stdin_pipe.write_fd, true)))
            goto cleanup_on_error;
        if (out_stream && !(*out_stream = inputstream_new_from_fd(stdout_pipe.read_fd, true)))
            goto cleanup_on_error;
        if (err_stream && !(*err_stream = inputstream_new_from_fd(stderr_pipe.read_fd, true)))
            goto cleanup_on_error;
        return true;
    }

cleanup_on_error:
    saved_errno = errno;
    close(stdin_pipe.read_fd);
    close(stdin_pipe.write_fd);
    close(stdout_pipe.read_fd);
    close(stdout_pipe.write_fd);
    close(stderr_pipe.read_fd);
    close(stderr_pipe.write_fd);
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
