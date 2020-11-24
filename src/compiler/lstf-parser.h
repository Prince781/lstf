#pragma once

#include "lstf-scanner.h"
#include "lstf-sourceref.h"
#include "lstf-file.h"

struct _lstf_parser {
    lstf_scanner *scanner;
    lstf_file *file;
    unsigned num_errors;
};
typedef struct _lstf_parser lstf_parser;

lstf_parser *lstf_parser_create(lstf_file *file);

/**
 * Parses file(s) in context.
 */
void lstf_parser_parse(lstf_parser *parser);

void lstf_parser_destroy(lstf_parser *parser);
