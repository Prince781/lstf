#include <stdio.h>
#include "compiler/lstf-scanner.h"
#include "lstf-file.h"

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
    printf("loaded %s. scanning ...\n", argv[1]);
    lstf_scanner *scanner = lstf_scanner_create(script);
    printf("done scanning.\n");
    lstf_scanner_destroy(scanner);
    return 0;
}
