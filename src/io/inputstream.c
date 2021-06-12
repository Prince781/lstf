#include "inputstream.h"
#include "io-common.h"
#include "data-structures/string-builder.h"
#include "outputstream.h"
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

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

    if (stream->ostream)
        outputstream_unref(stream->ostream);

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

inputstream *inputstream_new_from_outputstream(outputstream *ostream)
{
    switch (ostream->stream_type) {
    case outputstream_type_file:
    {
        inputstream *istream = inputstream_new_from_file(ostream->file, false);
        istream->ostream = outputstream_ref(ostream);
        return istream;
    }
    case outputstream_type_buffer:
    {
        inputstream *istream = inputstream_new_from_buffer(ostream->buffer, ostream->buffer_size, false);
        istream->ostream = outputstream_ref(ostream);
        return istream;
    }
    }

    fprintf(stderr, "%s: unexpected stream type `%u'\n", __func__, ostream->stream_type);
    abort();
}

static inline bool
inputstream_update_from_outputstream(inputstream *stream)
{
    if (stream->ostream) {
        long last_write_pos = -1;
        switch (stream->ostream->stream_type) {
        case outputstream_type_file:
            if ((last_write_pos = ftell(stream->file)) < 0)
                return false;
            stream->last_write_pos = last_write_pos;
            if (fseek(stream->file, stream->last_read_pos, SEEK_SET) < 0)
                return false;
            break;
        case outputstream_type_buffer:
            // realloc() can change the address of ostream's buffer
            stream->buffer = stream->ostream->buffer;
            stream->buffer_size = stream->ostream->buffer_offset;
            break;
        }
    }

    return true;
}

static inline bool
inputstream_restore_underlying_outputstream(inputstream *stream)
{
    if (stream->ostream) {
        switch (stream->ostream->stream_type) {
        case outputstream_type_file:
            // restore the write position of the stream when
            // inputstream_update_from_outputstream() was called
            if (fseek(stream->file, stream->last_write_pos, SEEK_SET) < 0)
                return false;
            break;
        case outputstream_type_buffer:
            break;
        }
    }

    return true;
}

char inputstream_read_char(inputstream *stream)
{
    if (!inputstream_update_from_outputstream(stream))
        return EOF;
    char read_char = EOF;
    switch (stream->stream_type) {
    case inputstream_type_file:
        read_char = fgetc(stream->file);
        inputstream_restore_underlying_outputstream(stream);
        return read_char;
    case inputstream_type_buffer:
        if (stream->buffer_offset >= stream->buffer_size) {
            inputstream_restore_underlying_outputstream(stream);
            return EOF;
        }
        inputstream_restore_underlying_outputstream(stream);
        return stream->buffer[stream->buffer_offset++];
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}

bool inputstream_unread_char(inputstream *stream)
{
    if (!inputstream_update_from_outputstream(stream))
        return false;
    switch (stream->stream_type) {
    case inputstream_type_file:
    {
        bool retval = !fseek(stream->file, -1, SEEK_CUR);
        inputstream_restore_underlying_outputstream(stream);
        return retval;
    }
    case inputstream_type_buffer:
        if (stream->buffer_offset == 0) {
            errno = EINVAL;
            inputstream_restore_underlying_outputstream(stream);
            return false;
        }
        stream->buffer_offset--;
        inputstream_restore_underlying_outputstream(stream);
        return true;
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}

bool inputstream_read_uint32(inputstream *stream, uint32_t *integer)
{
    uint32_t value = 0;

    for (unsigned i = 0; i < sizeof value; i++) {
        if (!inputstream_has_data(stream))
            return false;
        uint8_t byte = inputstream_read_char(stream);
        value |= ((uint32_t)byte) << (sizeof(value) - 1 - i) * CHAR_BIT;
    }

    *integer = value;
    return true;
}

bool inputstream_read_uint64(inputstream *stream, uint64_t *integer)
{
    uint64_t value = 0;

    for (unsigned i = 0; i < sizeof value; i++) {
        if (!inputstream_has_data(stream))
            return false;
        uint8_t byte = inputstream_read_char(stream);
        value |= ((uint64_t)byte) << (sizeof(value) - 1 - i) * CHAR_BIT;
    }

    *integer = value;
    return true;
}

size_t inputstream_read(inputstream *stream, void *buffer, size_t buffer_size)
{
    if (!inputstream_update_from_outputstream(stream))
        return 0;
    switch (stream->stream_type) {
    case inputstream_type_file:
    {
        size_t amt_read = fread(buffer, 1, buffer_size, stream->file);
        inputstream_restore_underlying_outputstream(stream);
        return amt_read;
    }
    case inputstream_type_buffer:
    {
        size_t remaining_amt = stream->buffer_size - stream->buffer_offset;
        size_t amt_read = buffer_size < remaining_amt ? buffer_size : remaining_amt;
        memcpy(buffer, &stream->static_buffer[stream->buffer_offset], amt_read);
        stream->buffer_offset += amt_read;
        inputstream_restore_underlying_outputstream(stream);
        return amt_read;
    }
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}

bool inputstream_skip(inputstream *stream, size_t bytes)
{
    if (!inputstream_update_from_outputstream(stream))
        return false;
    switch (stream->stream_type) {
    case inputstream_type_file:
    {
        bool retval = !fseek(stream->file, bytes, SEEK_CUR);
        inputstream_restore_underlying_outputstream(stream);
        return retval;
    }
    case inputstream_type_buffer:
        if (stream->buffer_size - stream->buffer_offset < bytes) {
            errno = ESPIPE;
            inputstream_restore_underlying_outputstream(stream);
            return false;
        } else {
            stream->buffer_offset += bytes;
        }
        inputstream_restore_underlying_outputstream(stream);
        return true;
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}

bool inputstream_has_data(inputstream *stream)
{
    if (!inputstream_update_from_outputstream(stream))
        return false;
    switch (stream->stream_type) {
    case inputstream_type_file:
    {
        if (feof(stream->file)) {
            inputstream_restore_underlying_outputstream(stream);
            return false;
        }
        
        if (!stream->ostream) {
            // it's possible we could have not performed a first read, in which
            // case we must do that to determine whether there is data available.
            // NOTE: ungetc() will be undone by fseek(), so we can only perform this
            // test if there is no underlying outputstream.
            char byte = 0;
            if ((byte = fgetc(stream->file)) == EOF)
                return false;
            // otherwise, unget character
            ungetc(byte, stream->file);
        }

        inputstream_restore_underlying_outputstream(stream);
        return true;
    }
    case inputstream_type_buffer:
        return stream->buffer_offset < stream->buffer_size;
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}

char *inputstream_get_name(inputstream *stream)
{
    // we don't care about failure to seek the input stream here
    inputstream_update_from_outputstream(stream);
    switch (stream->stream_type) {
    case inputstream_type_file:
        return io_get_filename_from_fd(fileno(stream->file));
    case inputstream_type_buffer:
        return string_destroy(string_appendf(string_new(), 
                    "<inputstream: buffer @ 0x%p>", (void *)stream->buffer)); 
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
    }

    fprintf(stderr, "%s: unreachable code: unexpected stream type `%u'\n", __func__, stream->stream_type);
    abort();
}
