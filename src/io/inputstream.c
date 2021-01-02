#include "inputstream.h"
#include "data-structures/string-builder.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if (_WIN32 || _WIN64)
#include <windows.h>
#include <io.h>
static char *get_filename_from_fd(int fd) {
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
#else
#include <unistd.h>
#include <limits.h>
static char *get_filename_from_fd(int fd)
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
#endif

inputstream *inputstream_ref(inputstream *stream)
{
    if (!stream)
        return NULL;

    assert(stream->floating || stream->refcount > 0);

    if (stream->floating) {
        stream->floating = false;
        stream->refcount = 1;
    } else {
        stream->refcount++;
    }

    return stream;
}

static void inputstream_free(inputstream *stream)
{
    switch (stream->stream_type) {
    case inputstream_type_file:
        if (stream->close_or_free_on_destruction)
            fclose(stream->file);
        break;
    case inputstream_type_buffer:
        if (stream->close_or_free_on_destruction)
            free(stream->buffer);
        break;
    }

    free(stream);
}

void inputstream_unref(inputstream *stream)
{
    if (!stream)
        return;

    assert(stream->floating || stream->refcount > 0);
    if (stream->floating || --stream->refcount == 0)
        inputstream_free(stream);
}

inputstream *inputstream_new_from_file(FILE *file, bool fclose_on_destroy)
{
    if (!file)
        return NULL;

    inputstream *stream = calloc(1, sizeof *stream);

    if (!stream) {
        int last_errno = errno;
        if (fclose_on_destroy)
            fclose(file);
        errno = last_errno;
        return NULL;
    }

    stream->stream_type = inputstream_type_file;
    stream->floating = true;
    stream->file = file;
    stream->close_or_free_on_destruction = fclose_on_destroy;

    return stream;
}

inputstream *inputstream_new_from_path(const char *path, const char *mode)
{
    return inputstream_new_from_file(fopen(path, mode), true);
}

inputstream *inputstream_new_from_buffer(void *buffer, size_t buffer_size, bool free_on_destroy)
{
    if (!buffer)
        return NULL;

    inputstream *stream = calloc(1, sizeof *stream);

    if (!stream) {
        int last_errno = errno;
        if (free_on_destroy)
            free(buffer);
        errno = last_errno;
        return NULL;
    }

    stream->stream_type = inputstream_type_buffer;
    stream->floating = true;
    stream->buffer = buffer;
    stream->close_or_free_on_destruction = free_on_destroy;
    stream->buffer_size = buffer_size;

    return stream;
}

inputstream *inputstream_new_from_string(char *str, bool free_on_destroy)
{
    return inputstream_new_from_buffer(str, strlen(str), free_on_destroy);
}

inputstream *inputstream_new_from_const_buffer(const void *buffer, size_t buffer_size)
{
    if (!buffer)
        return NULL;

    inputstream *stream = calloc(1, sizeof *stream);

    if (!stream)
        return NULL;

    stream->stream_type = inputstream_type_buffer;
    stream->floating = true;
    stream->const_buffer = buffer;
    stream->buffer_size = buffer_size;

    return stream;
}

inputstream *inputstream_new_from_const_string(const char *str)
{
    return inputstream_new_from_const_buffer(str, strlen(str));
}

char inputstream_read_char(inputstream *stream)
{
    switch (stream->stream_type) {
    case inputstream_type_file:
        return fgetc(stream->file);
    case inputstream_type_buffer:
        if (stream->buffer_offset >= stream->buffer_size)
            return EOF;
        return stream->buffer[stream->buffer_offset++];
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%d'\n", __func__, stream->stream_type);
    abort();
}

int inputstream_unread_char(inputstream *stream)
{
    switch (stream->stream_type) {
    case inputstream_type_file:
        return fseek(stream->file, -1, SEEK_CUR);
    case inputstream_type_buffer:
        if (stream->buffer_offset == 0) {
            errno = EINVAL;
            return EOF;
        }
        stream->buffer_offset--;
        return 0;
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%d'\n", __func__, stream->stream_type);
    abort();
}

bool inputstream_has_data(inputstream *stream)
{
    switch (stream->stream_type) {
    case inputstream_type_file:
        return !feof(stream->file);
    case inputstream_type_buffer:
        return stream->buffer_offset < stream->buffer_size;
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%d'\n", __func__, stream->stream_type);
    abort();
}

char *inputstream_get_name(inputstream *stream)
{
    switch (stream->stream_type) {
    case inputstream_type_file:
        return get_filename_from_fd(fileno(stream->file));
    case inputstream_type_buffer:
        return string_destroy(string_appendf(string_new(), 
                    "<inputstream: buffer @ 0x%p>", (void *)stream->buffer)); 
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%d'\n", __func__, stream->stream_type);
    abort();
}
