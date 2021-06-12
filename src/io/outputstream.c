#include "outputstream.h"
#include "io-common.h"
#include "util.h"
#include "data-structures/string-builder.h"
#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

outputstream *outputstream_ref(outputstream *stream)
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

static void outputstream_free(outputstream *stream)
{
    switch (stream->stream_type) {
    case outputstream_type_file:
        if (stream->close_or_free_on_destruction)
            fclose(stream->file);
        break;
    case outputstream_type_buffer:
        if (stream->close_or_free_on_destruction)
            free(stream->buffer);
        break;
    }
    free(stream);
}

void outputstream_unref(outputstream *stream)
{
    if (!stream)
        return;

    assert(stream->floating || stream->refcount > 0);
    if (stream->floating || --stream->refcount == 0)
        outputstream_free(stream);
}

outputstream *outputstream_new_from_file(FILE *file, bool fclose_on_destroy)
{
    if (!file)
        return NULL;

    outputstream *stream = calloc(1, sizeof *stream);

    if (!stream) {
        int last_errno = errno;
        if (fclose_on_destroy)
            fclose(file);
        errno = last_errno;
        return NULL;
    }

    stream->stream_type = outputstream_type_file;
    stream->floating = true;
    stream->close_or_free_on_destruction = fclose_on_destroy;
    stream->file = file;

    return stream;
}

outputstream *outputstream_new_from_path(const char *path, const char *mode)
{
    return outputstream_new_from_file(fopen(path, mode), true);
}

outputstream *outputstream_new_from_buffer(void *buffer, size_t initial_size, bool free_on_destroy)
{
    bool created_buffer = false;

    if (!buffer) {
        buffer = malloc(initial_size ? initial_size : (initial_size = BUFSIZ));
        if (!buffer)
            return NULL;
        created_buffer = true;
    }

    outputstream *stream = calloc(1, sizeof *stream);

    if (!stream) {
        if (free_on_destroy || created_buffer)
            free(buffer);
        return NULL;
    }

    stream->stream_type = outputstream_type_buffer;
    stream->floating = true;
    stream->close_or_free_on_destruction = free_on_destroy;
    stream->buffer = buffer;
    stream->buffer_size = initial_size;

    return stream;
}

static bool outputstream_resize_buffer(outputstream *stream, size_t minimum_new_size)
{
    if (stream->stream_type != outputstream_type_buffer)
        return false;

    if (stream->buffer_size >= minimum_new_size)
        return true;

    if (!stream->close_or_free_on_destruction) {
        // we can't call realloc() on a buffer we don't own.
        // this means that we've exhausted the buffer space.
        errno = ENOBUFS;
        return false;
    }

    if (minimum_new_size < stream->buffer_size * 2)
        minimum_new_size = stream->buffer_size * 2;

    void *new_buffer = realloc(stream->buffer, minimum_new_size);
    if (!new_buffer)
        return false;

    stream->buffer = new_buffer;
    stream->buffer_size = minimum_new_size;

    return true;
}

#define outputstream_write_integer(stream, integer, hton)\
{\
    integer = hton(integer);\
    switch (stream->stream_type) {\
    case outputstream_type_file:\
        return fwrite(&integer, sizeof integer, 1, stream->file);\
    case outputstream_type_buffer:\
        if (stream->buffer_offset + sizeof integer >= stream->buffer_size) {\
            if (!outputstream_resize_buffer(stream, stream->buffer_offset + sizeof integer))\
                return 0;\
        }\
        memcpy(stream->buffer + stream->buffer_offset, &integer, sizeof integer);\
        stream->buffer_offset += sizeof integer;\
        return sizeof integer;\
    }\
\
    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);\
    abort();\
}

static inline uint8_t htonb(uint8_t hostint) { return hostint; }

size_t outputstream_write_byte(outputstream *stream, uint8_t byte)
{
    outputstream_write_integer(stream, byte, htonb);
}

size_t outputstream_write_uint64(outputstream *stream, uint64_t integer)
{
    outputstream_write_integer(stream, integer, htonll);
}

size_t outputstream_write_int64(outputstream *stream, int64_t integer)
{
    return outputstream_write_uint64(stream, integer);
}

size_t outputstream_write_uint32(outputstream *stream, uint32_t integer)
{
    outputstream_write_integer(stream, integer, htonl);
}

size_t outputstream_write_string(outputstream *stream, const char *str)
{
    size_t string_length = strlen(str);
    switch (stream->stream_type) {
    case outputstream_type_file:
        return fwrite(str, 1, string_length, stream->file);
    case outputstream_type_buffer:
        if (stream->buffer_offset + string_length >= stream->buffer_size) {
            if (!outputstream_resize_buffer(stream, stream->buffer_offset + string_length))
                return 0;
        }
        memcpy(&stream->buffer[stream->buffer_offset], str, string_length);
        stream->buffer_offset += string_length;
        return string_length;
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}

size_t outputstream_write(outputstream *stream, void *buffer, size_t buffer_size)
{
    switch (stream->stream_type) {
    case outputstream_type_file:
        return fwrite(buffer, buffer_size, 1, stream->file);
    case outputstream_type_buffer:
        if (stream->buffer_offset + buffer_size >= stream->buffer_size) {
            if (!outputstream_resize_buffer(stream, stream->buffer_offset + buffer_size))
                return 0;
        }
        memcpy(&stream->buffer[stream->buffer_offset], buffer, buffer_size);
        stream->buffer_offset += buffer_size;
        return buffer_size;
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}

size_t outputstream_printf(outputstream *stream, const char *format, ...)
{
    string *temp_string = string_new();
    size_t amt_written = 0;
    va_list args;

    va_start(args, format);
    string_append_va(temp_string, format, args);
    va_end(args);

    amt_written = outputstream_write_string(stream, temp_string->buffer);

    string_unref(temp_string);
    return amt_written;
}

char *outputstream_get_name(outputstream *stream)
{
    switch (stream->stream_type) {
    case outputstream_type_file:
        return io_get_filename_from_fd(fileno(stream->file));
    case outputstream_type_buffer:
        return string_destroy(string_appendf(string_new(), 
                    "<outputstream: buffer @ 0x%p>", (void *)stream->buffer)); 
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}

int outputstream_get_fd(outputstream *stream)
{
    switch (stream->stream_type) {
    case outputstream_type_file:
        return fileno(stream->file);
    case outputstream_type_buffer:
        return -1;
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}
