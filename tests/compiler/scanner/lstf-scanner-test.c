#include <stdio.h>
#include "compiler/lstf-scanner.h"
#include "compiler/lstf-file.h"

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s script.lstf\n", argv[0]);
        return 1;
    }
    lstf_file *script = lstf_file_load(argv[1]);
    lstf_scanner *scanner = lstf_scanner_create(script);
    int num_errors = scanner->num_errors;
    lstf_scanner_destroy(scanner);
    lstf_file_unload(script);
    return num_errors ? 1 : 0;
}
