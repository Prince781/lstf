#pragma once

#include "lstf-scanner.h"
#include "lstf-sourceref.h"
#include "lstf-file.h"
#include <stdbool.h>
#include <limits.h>

struct _lstf_parser {
    unsigned refcount : sizeof(unsigned) * CHAR_BIT - 1;
    bool floating : 1;
    unsigned num_errors;
    lstf_scanner *scanner;
    lstf_file *file;
};
typedef struct _lstf_parser lstf_parser;

lstf_parser *lstf_parser_new(lstf_file *file);

lstf_parser *lstf_parser_ref(lstf_parser *parser);

void lstf_parser_unref(lstf_parser *parser);

/**
 * Parses file(s) in context.
 */
void lstf_parser_parse(lstf_parser *parser);
