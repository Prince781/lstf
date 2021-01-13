#include "outputstream.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
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
    if (!buffer) {
        buffer = malloc(initial_size ? initial_size : (initial_size = BUFSIZ));
        if (!buffer)
            return NULL;
    }

    outputstream *stream = calloc(1, sizeof *stream);

    if (!stream) {
        int last_errno = errno;
        if (free_on_destroy)
            free(buffer);
        errno = last_errno;
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

    if (minimum_new_size < stream->buffer_size * 2)
        minimum_new_size = stream->buffer_size * 2;

    void *new_buffer = realloc(stream->buffer, minimum_new_size);
    if (!new_buffer)
        return false;

    stream->buffer = new_buffer;
    stream->buffer_size = minimum_new_size;

    return true;
}

#define int_ref(i) _Generic((i), uint64_t: &(uint64_t){i}, uint32_t: &(uint32_t){i}, uint8_t: &(uint8_t){i})

#define outputstream_write_integer(stream, integer)\
{\
    switch (stream->stream_type) {\
    case outputstream_type_file:\
        return fwrite(int_ref(hton(integer)), sizeof integer, 1, stream->file);\
    case outputstream_type_buffer:\
        if (stream->buffer_offset + sizeof integer >= stream->buffer_size) {\
            if (!outputstream_resize_buffer(stream, stream->buffer_offset + sizeof integer))\
                return 0;\
        }\
        memcpy(stream->buffer + stream->buffer_offset, int_ref(hton(integer)), sizeof integer);\
        stream->buffer_offset += sizeof integer;\
        return sizeof integer;\
    }\
\
    fprintf(stderr, "%s: unreachable code: unexpected stream type `%d'\n", __func__, stream->stream_type);\
    abort();\
}

size_t outputstream_write_byte(outputstream *stream, uint8_t byte)
{
    outputstream_write_integer(stream, byte);
}

size_t outputstream_write_uint64(outputstream *stream, uint64_t integer)
{
    outputstream_write_integer(stream, integer);
}

size_t outputstream_write_int64(outputstream *stream, int64_t integer)
{
    return outputstream_write_uint64(stream, integer);
}

size_t outputstream_write_uint32(outputstream *stream, uint32_t integer)
{
    outputstream_write_integer(stream, integer);
}

size_t outputstream_write_string(outputstream *stream, const char *str)
{
    size_t string_length = strlen(str);
    switch (stream->stream_type) {
    case outputstream_type_file:
        return fwrite(str, string_length, 1, stream->file);
    case outputstream_type_buffer:
        if (stream->buffer_offset + string_length >= stream->buffer_size) {
            if (!outputstream_resize_buffer(stream, stream->buffer_offset + string_length))
                return 0;
        }
        memcpy(&stream->buffer[stream->buffer_offset], str, string_length);
        stream->buffer_offset += string_length;
        return string_length;
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%d'\n", __func__, stream->stream_type);
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

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%d'\n", __func__, stream->stream_type);
    abort();
}
