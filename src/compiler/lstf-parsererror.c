#include <stdarg.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "data-structures/ptr-list.h"
#include "data-structures/string-builder.h"
#include "lstf-parsererror.h"

static void lstf_parsernote_destroy(lstf_parsernote *note);

lstf_parsererror *lstf_parsererror_new(const lstf_sourceref *source_reference,
                                       const char           *format,
                                       ...)
{
    lstf_parsererror *error = calloc(1, sizeof *error);

    if (!error) {
        perror("could not allocate new parser error");
        abort();
    }

    error->source_reference = *source_reference;

    va_list args;

    va_start(args, format);
    error->message = string_destroy(string_append_va(string_new(), format, args));
    va_end(args);

    error->notes = ptr_list_new(NULL, (collection_item_unref_func) lstf_parsernote_destroy);

    return error;
}

void lstf_parsererror_add_note(lstf_parsererror     *error,
                               const lstf_sourceref *source_reference,
                               const char           *format,
                               ...)
{
    lstf_parsernote *note = calloc(1, sizeof *note);

    if (!note) {
        perror("could not allocate parser note");
        abort();
    }

    note->source_reference = *source_reference;
    va_list args;

    va_start(args, format);
    note->message = string_destroy(string_append_va(string_new(), format, args));
    va_end(args);

    ptr_list_append(error->notes, note);
}

static void lstf_parsernote_destroy(lstf_parsernote *note)
{
    free(note->message);
    free(note);
}

void lstf_parsererror_destroy(lstf_parsererror *error)
{
    ptr_list_destroy(error->notes);
    free(error->message);
    free(error);
}
