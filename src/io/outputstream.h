#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "lstf-common.h"

enum _outputstream_type {
    outputstream_type_file,
    outputstream_type_buffer
};
typedef enum _outputstream_type outputstream_type;

struct _outputstream {
    outputstream_type stream_type;
    unsigned long refcount : sizeof(unsigned long) - (1 + 1);
    bool floating : 1;
    bool close_or_free_on_destruction : 1;
    union {
        FILE *file;
        struct {
            uint8_t *buffer;
            size_t buffer_offset;
            size_t buffer_size;
        };
    };
};
typedef struct _outputstream outputstream;

outputstream *outputstream_ref(outputstream *stream);

void outputstream_unref(outputstream *stream);

outputstream *outputstream_new_from_file(FILE *file, bool fclose_on_destroy);

outputstream *outputstream_new_from_path(const char *path, const char *mode);

outputstream *outputstream_new_from_buffer(void *buffer, size_t initial_size, bool free_on_destroy);

size_t outputstream_write_byte(outputstream *stream, uint8_t byte);

size_t outputstream_write_uint64(outputstream *stream, uint64_t integer);

size_t outputstream_write_int64(outputstream *stream, int64_t integer);

size_t outputstream_write_uint32(outputstream *stream, uint32_t integer);

size_t outputstream_write_string(outputstream *stream, const char *str);

size_t outputstream_write(outputstream *stream, const void *buffer, size_t buffer_size);

size_t outputstream_printf(outputstream *stream, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

char *outputstream_get_name(outputstream *stream);

/**
 * If this stream is backed by a file, returns the file descriptor. Otherwise,
 * returns -1.
 */
int outputstream_get_fd(outputstream *stream);
