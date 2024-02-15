#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include "event.h"

typedef struct _outputstream outputstream;

enum _inputstream_type {
    inputstream_type_file,
    inputstream_type_buffer,
    inputstream_type_fd
};
typedef enum _inputstream_type inputstream_type;

struct _inputstream {
    inputstream_type stream_type;
    unsigned long refcount : sizeof(unsigned long) - (1 + 1);
    bool floating : 1;
    bool close_or_free_on_destruction : 1;
    union {
        struct {        // inputstream_type_file
            FILE *file;
            long last_read_pos;
        };
        struct {        // inputstream_type_buffer
            union {
                uint8_t *buffer;
                const uint8_t *static_buffer;
            };
            size_t buffer_offset;
            size_t buffer_size;
        };
        struct {        // inputstream_type_fd
            int fd;
            uint8_t *fdbuffer;
            size_t fdbuffer_size;               // amount of data in buffer
            size_t fdbuffer_offset;
            size_t fdbuffer_capacity;           // max size of buffer
        };
    };
};
typedef struct _inputstream inputstream;

inputstream *inputstream_ref(inputstream *stream);

void inputstream_unref(inputstream *stream);

inputstream *inputstream_new_from_file(FILE *file, bool fclose_on_destroy);

inputstream *inputstream_new_from_path(const char *path, const char *mode);

inputstream *inputstream_new_from_buffer(void *buffer, size_t buffer_size, bool free_on_destroy);

/**
 * Creates a new input stream from a NUL-terminated string.
 */
inputstream *inputstream_new_from_string(char *str, bool free_on_destroy);

inputstream *inputstream_new_from_static_buffer(const void *buffer, size_t buffer_size);

inputstream *inputstream_new_from_static_string(const char *str);

/**
 * Creates a new input stream from a file descriptor.
 */
inputstream *inputstream_new_from_fd(int fd, bool close_on_destroy);

/**
 * Reads a character and advances the underlying stream pointer.
 *
 * @return whether the operation succeded
 */
bool inputstream_read_char(inputstream *stream, char *c);

/**
 * Unreads a character. Returns `true` on success, `false` on error.
 */
bool inputstream_unread_char(inputstream *stream, char c);

/**
 * Returns `true` and `integer` is set to the integer that was read. Otherwise
 * (if there was an error or EOF) returns `false`.
 */
bool inputstream_read_uint32(inputstream *stream, uint32_t *integer);

/**
 * Returns `true` and `integer` is set to the integer that was read. Otherwise
 * (if there was an error or EOF) returns `false`.
 */
bool inputstream_read_uint64(inputstream *stream, uint64_t *integer);

/**
 * Reads an arbitrary amount of data into `buffer`. Returns the amount of data read.
 * Returns 0 if there was an error or if the stream is empty.
 */
size_t inputstream_read(inputstream *stream, void *buffer, size_t buffer_size);

/**
 * Determines whether this has at least 1 byte that can be read synchronously.
 * This can happen because the buffer is not empty, or because of\bl
 */
bool inputstream_ready(inputstream *stream);

/**
 * Advances the stream by `bytes` bytes. Returns `true` if the operation was
 * successful, `false` otherwise.
 */
bool inputstream_skip(inputstream *stream, size_t bytes);

char *inputstream_get_name(inputstream *stream);

/**
 * Get the file descriptor, or -1 if this stream is not a file.
 */
int inputstream_get_fd(inputstream *stream);
