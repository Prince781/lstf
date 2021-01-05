#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

enum _inputstream_type {
    inputstream_type_file,
    inputstream_type_buffer
};
typedef enum _inputstream_type inputstream_type;

struct _inputstream {
    inputstream_type stream_type;
    unsigned long refcount : sizeof(unsigned long) - (1 + 1);
    bool floating : 1;
    bool close_or_free_on_destruction : 1;
    union {
        FILE *file;
        char *buffer;
        const char *const_buffer;
    };
    size_t buffer_offset;
    size_t buffer_size;
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

inputstream *inputstream_new_from_const_buffer(const void *buffer, size_t buffer_size);

inputstream *inputstream_new_from_const_string(const char *str);

/**
 * Reads a character and advances the underlying stream pointer.
 */
char inputstream_read_char(inputstream *stream);

/**
 * Unreads a character. Returns `EOF` on error.
 */
int inputstream_unread_char(inputstream *stream);

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
 * Reads an arbitrary amount of data into `buffer`. Returns `true` if the read
 * was successful, `false` otherwise.
 */
bool inputstream_read(inputstream *stream, void *buffer, size_t buffer_size);

bool inputstream_has_data(inputstream *stream);

char *inputstream_get_name(inputstream *stream);
