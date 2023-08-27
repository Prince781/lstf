#pragma once

#include "lstf-common.h"
#include "util.h"
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

struct _string {
    union {
        char *buffer;
        const char *const_buffer;
    };
    size_t buffer_size;
    size_t length;
    size_t refcount : sizeof(size_t)*CHAR_BIT - (1 + 1);
    bool floating : 1;
    bool copy_on_write : 1;
};
typedef struct _string string;

string *string_ref(string *sb);

void string_unref(string *sb);

/**
 * Creates an empty new string
 */
string *string_new(void);

/**
 * Creates a new string backing an existing string that will not be free()'d.
 */
string *string_new_with_static_data(const char *data);

/**
 * Creates a new string from a copy of `data`.
 */
string *string_new_copy_data(const char *data);

/**
 * Creates a new string that will take ownership of `data`, which will be
 * free()'d when this string is free()'d.
 */
string *string_new_take_data(char *data);

/**
 * Duplicates an existing `string` object.
 */
string *string_dup(string *sb);

string *string_append_va(string *sb, const char *format, va_list args)
    __attribute__((format (printf, 2, 0)));

string *string_appendf(string *sb, const char *format, ...)
    __attribute__((format (printf, 2, 3)));

/**
 * Creates a new string with a format.
 */
#define string_newf(format, ...) string_appendf(string_new(), format, __VA_ARGS__)

static inline bool string_is_empty(const string *sb) {
    return sb->length;
}

static inline bool string_hash(const string *sb) {
    return strhash(sb->const_buffer);
}

static inline bool string_is_equal_to(const string *sb1, const string *sb2) {
    return strcmp(sb1->const_buffer, sb2->const_buffer) == 0;
}

string *string_clear(string *sb);

/**
 * Destroys the string's metadata and returns the contents. You must `free()`
 * this when done.
 */
char *string_destroy(string *sb);
