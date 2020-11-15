#pragma once

#include "lstf-scanner.h"
#include "lstf-sourceref.h"
#include "lstf-file.h"

struct _lstf_parser {
    lstf_sourceloc source_location;
    lstf_scanner *scanner;
};
typedef struct _lstf_parser lstf_parser;

lstf_parser *lstf_parser_create(const lstf_file *script);

void lstf_parser_destroy(lstf_parser *parser);
