#include "inputstream.h"
#include "io-common.h"
#include "data-structures/string-builder.h"
#include "outputstream.h"
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#else
#include <unistd.h>
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
    if (stream->close_or_free_on_destruction) {
        switch (stream->stream_type) {
        case inputstream_type_file:
            fclose(stream->file);
            break;
        case inputstream_type_buffer:
            free(stream->buffer);
            break;
        case inputstream_type_fd:
            close(stream->fd);
            free(stream->fdbuffer);
            break;
        }
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
    if (!buffer) {
        errno = EINVAL;
        return NULL;
    }

    inputstream *stream = calloc(1, sizeof *stream);

    if (!stream) {
        if (free_on_destroy)
            free(buffer);
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

inputstream *inputstream_new_from_static_buffer(const void *buffer, size_t buffer_size)
{
    if (!buffer)
        return NULL;

    inputstream *stream = calloc(1, sizeof *stream);

    if (!stream)
        return NULL;

    stream->stream_type = inputstream_type_buffer;
    stream->floating = true;
    stream->static_buffer = buffer;
    stream->buffer_size = buffer_size;

    return stream;
}

inputstream *inputstream_new_from_static_string(const char *str)
{
    return inputstream_new_from_static_buffer(str, strlen(str));
}

inputstream *inputstream_new_from_fd(int fd, bool close_on_destroy)
{
    // note: we can't use fdopen() with inputstream_new_from_file() because we
    // can't close the FILE without also closing the file descriptor
    inputstream *stream = calloc(1, sizeof *stream);

    if (!stream)
        return NULL;

    size_t fdbuffer_capacity = 1024;
    uint8_t *fdbuffer = malloc(fdbuffer_capacity);
    if (!fdbuffer) {
        free(stream);
        return NULL;
    }

    stream->stream_type = inputstream_type_fd;
    stream->floating = true;
    stream->fd = fd;
    stream->fdbuffer = fdbuffer;
    stream->fdbuffer_capacity = fdbuffer_capacity;
    stream->close_or_free_on_destruction = close_on_destroy;

    return stream;
}

bool inputstream_read_char(inputstream *stream, char *c)
{
    int read_char = EOF;
    switch (stream->stream_type) {
    case inputstream_type_file:
        if ((read_char = fgetc(stream->file)) == EOF)
            return false;
        *c = read_char;
        return true;
    case inputstream_type_buffer:
        if (stream->buffer_offset >= stream->buffer_size)
            return false;
        *c = stream->buffer[stream->buffer_offset++];
        return true;
    case inputstream_type_fd:
        return inputstream_read(stream, c, sizeof *c) == 1;
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}

bool inputstream_unread_char(inputstream *stream, char c)
{
    switch (stream->stream_type) {
    case inputstream_type_file:
    {
        if (ungetc(c, stream->file) != c)
            return fseek(stream->file, -1, SEEK_CUR) == 0;
        return true;
    }
    case inputstream_type_buffer:
        if (stream->buffer_offset == 0) {
            errno = EINVAL;
            return false;
        }
        stream->buffer_offset--;
        return true;
    case inputstream_type_fd:
        if (stream->fdbuffer_offset == 0) {
            errno = EINVAL;
            return false;
        }
        stream->fdbuffer_offset--;
        return true;
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}

bool inputstream_read_uint32(inputstream *stream, uint32_t *integer)
{
    uint32_t value = 0;

    for (unsigned i = 0; i < sizeof value; i++) {
        char c;
        if (!inputstream_read_char(stream, &c))
            return false;
        uint8_t byte = c;
        value |= ((uint32_t)byte) << (sizeof(value) - 1 - i) * CHAR_BIT;
    }

    *integer = value;
    return true;
}

bool inputstream_read_uint64(inputstream *stream, uint64_t *integer)
{
    uint64_t value = 0;

    for (unsigned i = 0; i < sizeof value; i++) {
        char c;
        if (!inputstream_read_char(stream, &c))
            return false;
        uint8_t byte = c;
        value |= ((uint64_t)byte) << (sizeof(value) - 1 - i) * CHAR_BIT;
    }

    *integer = value;
    return true;
}

size_t inputstream_read(inputstream *stream, void *buffer, size_t buffer_size)
{
    switch (stream->stream_type) {
    case inputstream_type_file:
    {
        size_t amt_read = fread(buffer, 1, buffer_size, stream->file);
        return amt_read;
    }
    case inputstream_type_buffer:
    {
        size_t remaining_amt = stream->buffer_size - stream->buffer_offset;
        size_t amt_read = buffer_size < remaining_amt ? buffer_size : remaining_amt;
        memcpy(buffer, &stream->static_buffer[stream->buffer_offset], amt_read);
        stream->buffer_offset += amt_read;
        return amt_read;
    }
    case inputstream_type_fd:
    {
        if (stream->fdbuffer_offset >= stream->fdbuffer_size) {
            // we have to refill the buffer
            int amt = read(stream->fd, stream->fdbuffer, stream->fdbuffer_capacity);
            if (amt == -1)      // error
                return 0;
            stream->fdbuffer_offset = 0;
            stream->fdbuffer_size = amt;
        }
        size_t remaining_amt = stream->fdbuffer_size - stream->fdbuffer_offset;
        size_t amt_read = buffer_size < remaining_amt ? buffer_size : remaining_amt;
        memcpy(buffer, &stream->fdbuffer[stream->fdbuffer_offset], amt_read);
        stream->fdbuffer_offset += amt_read;
        return amt_read;
    }
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}

bool inputstream_ready(inputstream *stream)
{
    switch (stream->stream_type) {
    case inputstream_type_file:
        return false /* TODO */;
    case inputstream_type_buffer:
        return stream->buffer_offset < stream->buffer_size;
    case inputstream_type_fd:
        return stream->fdbuffer_offset < stream->fdbuffer_size;
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}

bool inputstream_skip(inputstream *stream, size_t bytes)
{
    switch (stream->stream_type) {
    case inputstream_type_file:
    {
        bool retval = !fseek(stream->file, bytes, SEEK_CUR);
        return retval;
    }
    case inputstream_type_buffer:
        if (stream->buffer_size - stream->buffer_offset < bytes) {
            errno = ENOMEM;
            return false;
        } else {
            stream->buffer_offset += bytes;
        }
        return true;
    case inputstream_type_fd:
        if (stream->fdbuffer_size - stream->fdbuffer_offset < bytes) {
            // first, set offset to end of buffer
            size_t seek_amt = bytes - (stream->fdbuffer_size - stream->fdbuffer_offset);
            stream->fdbuffer_offset = stream->fdbuffer_size;
            // now, seek the remaining amount
            if (lseek(stream->fd, (off_t)seek_amt, SEEK_CUR) == -1)
                return false;
            return true;
        } else {
            stream->fdbuffer_offset += bytes;
        }
        return true;
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}

char *inputstream_get_name(inputstream *stream)
{
    // we don't care about failure to seek the input stream here
    switch (stream->stream_type) {
    case inputstream_type_file:
        return io_get_filename_from_fd(fileno(stream->file));
    case inputstream_type_buffer:
        return string_destroy(string_appendf(string_new(), 
                    "<inputstream: buffer @ 0x%p>", (void *)stream->buffer)); 
    case inputstream_type_fd:
        return io_get_filename_from_fd(stream->fd);
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}

int inputstream_get_fd(inputstream *stream)
{
    switch (stream->stream_type) {
    case inputstream_type_file:
        return fileno(stream->file);
    case inputstream_type_buffer:
        return -1;
    case inputstream_type_fd:
        return stream->fd;
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}
