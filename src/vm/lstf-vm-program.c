#include "lstf-vm-program.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LSTFC_MAGIC_VALUE (char[]) { '\x89', 'L', 'S', 'T', 'F', '\x01', '\x0A', '\x00' }

lstf_vm_program *lstf_vm_program_load(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    lstf_vm_program *program = NULL;

    if (!file) {
        return NULL;
    }

    program = calloc(1, sizeof *program);
    program->floating = true;

    for (unsigned i = 0; i < sizeof(LSTFC_MAGIC_VALUE); i++) {
        // TODO: deserialize
    }

    fclose(file);
    return program;
}
