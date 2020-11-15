#pragma once

#include <stdarg.h>
#include <stddef.h>

struct _string {
    char *buffer;
    size_t buffer_size;
    size_t length;
};
typedef struct _string string;

string *string_new(void);

string *string_append_va(string *sb, const char *format, va_list args);

string *string_appendf(string *sb, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));

string *string_clear(string *sb);

char *string_destroy(string *sb);

void string_free(string *sb);
