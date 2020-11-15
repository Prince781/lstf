#include "lstf-parser.h"
#include "compiler/lstf-scanner.h"
#include <stdlib.h>

lstf_parser *lstf_parser_create(const lstf_file *script)
{
    lstf_parser *parser = calloc(1, sizeof *parser);

    if (!(parser->scanner = lstf_scanner_create(script))) {
        free(parser);
        return NULL;
    }
    parser->source_location.line = 1;

    return parser;
}

void lstf_parser_destroy(lstf_parser *parser)
{
    lstf_scanner_destroy(parser->scanner);
    parser->scanner = NULL;
    free(parser);
}
