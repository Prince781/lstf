#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "compiler/lstf-parser.h"
#include "compiler/lstf-report.h"
#include "compiler/lstf-scanner.h"
#include "compiler/lstf-file.h"
#include "compiler/lstf-symbolresolver.h"
#include "compiler/lstf-semanticanalyzer.h"

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

    lstf_parser *parser = lstf_parser_create(script);
    lstf_symbolresolver *resolver = lstf_symbolresolver_new(script);
    lstf_semanticanalyzer *analyzer = lstf_semanticanalyzer_new(script);
    int retval = 0;

    lstf_parser_parse(parser);
    if (parser->scanner->num_errors + parser->num_errors == 0) {
        lstf_symbolresolver_resolve(resolver);
        if (resolver->num_errors == 0) {
            lstf_semanticanalyzer_analyze(analyzer);
            if (analyzer->num_errors == 0) {
            } else {
                fprintf(stderr, "%u error(s) generated.\n", analyzer->num_errors);
                retval = 1;
            }
        } else {
            fprintf(stderr, "%u error(s) generated.\n", resolver->num_errors);
            retval = 1;
        }
    } else {
        fprintf(stderr, "%u error(s) generated.\n", parser->scanner->num_errors + parser->num_errors);
        retval = 1;
    }

    lstf_parser_destroy(parser);
    lstf_symbolresolver_destroy(resolver);
    lstf_semanticanalyzer_destroy(analyzer);
    lstf_file_unload(script);
    return retval;
}
