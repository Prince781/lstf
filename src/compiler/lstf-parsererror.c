#include "json/json-parser.h"
#include <stdarg.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "data-structures/string-builder.h"
#include "lstf-parsererror.h"

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

    return error;
}

void lstf_parsererror_destroy(lstf_parsererror *error)
{
    free(error->message);
    free(error);
}
