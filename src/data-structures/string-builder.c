#include "string-builder.h"
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static void string_free(string *sb)
{
    assert(sb->refcount == 0 && "cannot free string held by other owners!");

    if (!sb->copy_on_write)
        free(sb->buffer);
    sb->buffer = NULL;
    sb->buffer_size = 0;
    sb->length = 0;
    free(sb);
}

string *string_ref(string *sb)
{
    if (sb) {
        if (sb->floating) {
            sb->floating = false;
            sb->refcount = 1;
        } else {
            sb->refcount++;
        }
    }

    return sb;
}

void string_unref(string *sb)
{
    if (!sb)
        return;
    assert(sb->floating || sb->refcount > 0);
    if (sb->floating || --sb->refcount == 0)
        string_free(sb);
}

string *string_new(void)
{
    string *sb = calloc(1, sizeof *sb);

    if (!sb) {
        perror("failed to create string");
        abort();
    }

    const size_t initial_size = 8;
    sb->buffer = calloc(initial_size, sizeof *sb->buffer);
    if (!sb->buffer) {
        free(sb);
        perror("failed to create string buffer");
        abort();
    }
    sb->buffer_size = initial_size;
    sb->floating = true;

    return sb;
}

string *string_new_with_static_data(const char *data)
{
    string *sb = calloc(1, sizeof *sb);

    if (!sb) {
        perror("failed to create string");
        abort();
    }

    sb->const_buffer = data;
    sb->copy_on_write = true;
    sb->buffer_size = 0;
    sb->floating = true;

    return sb;
}

string *string_new_copy_data(const char *data)
{
    string *sb = calloc(1, sizeof *sb);

    if (!sb) {
        perror("failed to create string");
        abort();
    }

    if (!(sb->buffer = strdup(data))) {
        perror("failed to create string buffer from copied data");
        abort();
    }
    sb->buffer_size = strlen(sb->buffer);
    sb->floating = true;

    return sb;
}

string *string_new_take_data(char *data)
{
    string *sb = calloc(1, sizeof *sb);

    if (!sb) {
        perror("failed to create string");
        abort();
    }

    sb->buffer = data;
    sb->buffer_size = strlen(data);
    sb->floating = true;

    return sb;
}

string *string_dup(string *sb)
{
    string *new_sb = string_new_with_static_data(sb->buffer);
    sb->copy_on_write = true;
    return new_sb;
}

string *string_append_va(string *sb, const char *format, va_list args)
{
    int required_size = 0;
    va_list saved_args;

    va_copy(saved_args, args);
    required_size = vsnprintf(NULL, 0, format, args) + 1 /* for NUL character */;

    if (required_size > 0) {
        if (sb->copy_on_write) {
            sb->buffer = strdup(sb->const_buffer);
            sb->length = strlen(sb->buffer);
            sb->buffer_size = sb->length + 1;
            sb->copy_on_write = false;
        }
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
        sb->length += (size_t)vsnprintf(sb->buffer + sb->length, 
                (size_t)required_size, format, saved_args);
        sb->buffer[sb->length] = '\0';
    }

    va_end(saved_args);

    return sb;
}

string *string_appendf(string *sb, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    sb = string_append_va(sb, format, args);
    va_end(args);

    return sb;
}

string *string_clear(string *sb)
{
    sb->length = 0;
    sb->buffer_size = 8;
    if (sb->copy_on_write) {
        char *new_buffer = calloc(sb->buffer_size, sizeof *sb->buffer);
        // tolerate failure
        sb->buffer = new_buffer ? new_buffer : sb->buffer;
        sb->copy_on_write = false;
    } else {
        sb->buffer = realloc(sb->buffer, sb->buffer_size);
    }
    sb->buffer[sb->length] = '\0';
    return sb;
}

char *string_destroy(string *sb)
{
    if (!sb)
        return NULL;
    assert(sb->refcount == 0 && "cannot destroy string held by other owners!");

    const size_t length = sb->length;
    const bool copy_on_write = sb->copy_on_write;
    char *buffer = copy_on_write ? NULL : sb->buffer;
    const char *const_buffer = copy_on_write ? sb->const_buffer : NULL;

    sb->buffer = NULL;
    sb->buffer_size = 0;
    sb->length = 0;
    free(sb);

    if (copy_on_write) {
        if (!(buffer = strdup(const_buffer))) {
            perror("failed to copy const buffer on string destruction");
            abort();
        }
    } else {
        char *new_buffer = realloc(buffer, length + 1);
        if (new_buffer) {
            new_buffer[length] = '\0';
            buffer = new_buffer;
        } else {
            buffer[length] = '\0';
        }
    }

    return buffer;
}
