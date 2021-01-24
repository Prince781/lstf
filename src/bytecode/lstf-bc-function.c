#include "lstf-bc-function.h"
#include "lstf-bc-instruction.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

lstf_bc_function *lstf_bc_function_new(const char *name)
{
    lstf_bc_function *function = calloc(1, sizeof *function);

    if (!function) {
        perror("failed to create lstf_bc_function");
        abort();
    }

    if (!(function->name = strdup(name))) {
        perror("failed to save function name");
        abort();
    }
    function->instructions_bufsize = 1024;
    function->instructions = calloc(function->instructions_bufsize, sizeof *function->instructions);

    if (!function->instructions) {
        perror("failed to create buffer for lstf_bc_function instructions");
        abort();
    }

    return function;
}

void lstf_bc_function_destroy(lstf_bc_function *function)
{
    free(function->name);
    function->name = NULL;
    for (unsigned long i = 0; i < function->instructions_length; i++)
        lstf_bc_instruction_clear(&function->instructions[i]);
    free(function->instructions);
    function->instructions = NULL;
    function->instructions_bufsize = 0;
    function->instructions_length = 0;
    free(function);
}

lstf_bc_instruction *lstf_bc_function_add_instruction(lstf_bc_function   *function,
                                                      lstf_bc_instruction instruction)
{
    if (function->instructions_length >= function->instructions_bufsize) {
        const size_t new_bufsize = function->instructions_bufsize + function->instructions_bufsize / 2;
        lstf_bc_instruction *new_instructions = realloc(function->instructions,
                new_bufsize * sizeof(*function->instructions));
        if (!new_instructions) {
            fprintf(stderr, "%s: failed to resize instructions buffer - %s\n", 
                    __func__, strerror(errno)); 
            abort();
        }
        function->instructions = new_instructions;
        function->instructions_bufsize = new_bufsize;
    }

    lstf_bc_instruction *slot = &function->instructions[function->instructions_length++];
    *slot = instruction;
    return slot;
}
