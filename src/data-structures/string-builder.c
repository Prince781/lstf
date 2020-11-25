#include "string-builder.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

string *string_new(void)
{
    string *sb = calloc(1, sizeof *sb);

    if (!sb) {
        perror("failed to allocate string builder");
        abort();
    }

    const unsigned initial_size = 8;
    sb->buffer = calloc(initial_size, sizeof *sb->buffer);
    if (!sb->buffer) {
        perror("failed to allocate string builder buffer");
        abort();
    }
    sb->buffer_size = initial_size;

    return sb;
}

string *string_append_va(string *sb, const char *format, va_list args)
{
    int required_size = 0;
    va_list saved_args;

    va_copy(saved_args, args);
    required_size = vsnprintf(NULL, 0, format, args) + 1;

    if (required_size > 0) {
        if ((size_t)required_size > sb->buffer_size - sb->length) {
            size_t new_size = sb->length + (size_t)required_size;
            if (new_size < sb->buffer_size + sb->buffer_size/2)
                new_size = sb->buffer_size + sb->buffer_size/2;
            sb->buffer = realloc(sb->buffer, new_size);
            sb->buffer_size = new_size;
            if (!sb->buffer) {
                perror("failed to resize string builder buffer");
                abort();
            }
        }
        sb->length += (size_t)vsnprintf(sb->buffer + sb->length, (size_t)required_size, format, saved_args);
        sb->buffer[sb->length] = '\0';
    }

    va_end(saved_args);

    return sb;
}

string *string_appendf(string *sb, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    string_append_va(sb, format, args);
    va_end(args);

    return sb;
}

string *string_clear(string *sb)
{
    sb->length = 0;
    sb->buffer_size = 8;
    sb->buffer = realloc(sb->buffer, sb->buffer_size);
    sb->buffer[sb->length] = '\0';
    return sb;
}

char *string_destroy(string *sb)
{
    char *buffer = sb->buffer;
    const size_t length = sb->length;

    sb->buffer = NULL;
    sb->buffer_size = 0;
    sb->length = 0;
    free(sb);

    if (!(buffer = realloc(buffer, length + 1))) {
        perror("could not resize string builder buffer");
        abort();
    }
    buffer[length] = '\0';

    return buffer;
}

void string_free(string *sb)
{
    free(sb->buffer);
    sb->buffer = NULL;
    sb->buffer_size = 0;
    sb->length = 0;
    free(sb);
}
