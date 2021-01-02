#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum _outputstream_type {
    outputstream_type_file,
    outputstream_type_buffer
};
typedef enum _outputstream_type outputstream_type;

struct _outputstream {
    outputstream_type stream_type : 1;
    unsigned long refcount : sizeof(unsigned long) - (1 + 1 + 1);
    bool floating : 1;
    bool close_or_free_on_destruction : 1;
    union {
        FILE *file;
        uint8_t *buffer;
    };
    size_t buffer_offset;
    size_t buffer_size;
};
typedef struct _outputstream outputstream;

outputstream *outputstream_ref(outputstream *stream);

void outpustream_unref(outputstream *stream);

outputstream *outputstream_new_from_file(FILE *file, bool fclose_on_destroy);

outputstream *outputstream_new_from_path(const char *path, const char *mode);

outputstream *outputstream_new_from_buffer(void *buffer, size_t initial_size, bool free_on_destroy);

int outputstream_write_byte(outputstream *stream, uint8_t byte);

int outputstream_write_uint64(outputstream *stream, uint64_t integer);

int outputstream_write_int64(outputstream *stream, int64_t integer);

int outputstream_write_string(outputstream *stream, const char *str);
