#include <stdio.h>
#include "compiler/lstf-parser.h"
#include "compiler/lstf-scanner.h"
#include "compiler/lstf-file.h"

static void
print_usage(const char *progname)
{
    fprintf(stderr, "usage: %s script.lstf\n", progname);
}

int main(int argc, char *argv[])
{
    printf("lstf version 0.0.1\n");
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    lstf_file *script = lstf_file_load(argv[1]);
    printf("loaded %s. scanning and parsing ...\n", argv[1]);
    lstf_parser *parser = lstf_parser_create(script);
    lstf_parser_parse(parser);
    printf("done scanning and parsing.\n");
    lstf_parser_destroy(parser);
    lstf_file_unload(script);
    return 0;
}
