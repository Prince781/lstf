#include <stdio.h>
#include "compiler/lstf-parser.h"
#include "compiler/lstf-file.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s script.lstf\n", argv[0]);
        return 1;
    }
    lstf_file *script = lstf_file_load(argv[1]);
    lstf_parser *parser = lstf_parser_new(script);
    lstf_parser_parse(parser);
    unsigned num_errors = parser->num_errors;
    lstf_parser_unref(parser);
    return num_errors ? 1 : 0;
}
