#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "compiler/lstf-parser.h"
#include "compiler/lstf-report.h"
#include "compiler/lstf-scanner.h"
#include "compiler/lstf-file.h"
#include "compiler/lstf-symbolresolver.h"

static void
print_usage(const char *progname)
{
    fprintf(stderr, "usage: %s script.lstf\n", progname);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    lstf_file *script = lstf_file_load(argv[1]);
    if (!script) {
        lstf_report_error(NULL, "%s: %s", argv[1], strerror(errno));
        fprintf(stderr, "compilation terminated.\n");
        return 1;
    }
    printf("loaded %s. scanning and parsing ...\n", argv[1]);
    lstf_parser *parser = lstf_parser_create(script);
    lstf_parser_parse(parser);
    printf("...done scanning and parsing.\n");
    lstf_parser_destroy(parser);
    printf("performing symbol resolution ...\n");
    lstf_symbolresolver *resolver = lstf_symbolresolver_new(script);
    lstf_symbolresolver_resolve(resolver);
    lstf_symbolresolver_destroy(resolver);
    printf("...done resolving symbols\n");
    lstf_file_unload(script);
    return 0;
}
