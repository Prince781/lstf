#include "io/io-common.h"
#include "data-structures/string-builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
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
#else
#include <unistd.h>
#include <limits.h>
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
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <consoleapi.h>
#include <io.h>
bool is_ascii_terminal(FILE *file) {
    HANDLE file_handle = (HANDLE) _get_osfhandle(fileno(file));
    if (file_handle)
        return !!SetConsoleMode(file_handle, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    return false;
}
#else
#include <unistd.h>
bool is_ascii_terminal(FILE *file) {
    return isatty(fileno(file));
}
#endif
