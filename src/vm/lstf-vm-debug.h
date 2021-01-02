#pragma once

#include <stdint.h>

struct _lstf_vm_debugentry {
    /**
     * Beginning offset in the code section where this debug entry applies.
     */
    uint32_t instruction_offset;

    uint32_t source_line;
    uint32_t source_column;
};
typedef struct _lstf_vm_debugentry lstf_vm_debugentry;

struct _lstf_vm_debugsym {
    uint32_t instruction_offset;
    char name[];
};
typedef struct _lstf_vm_debugsym lstf_vm_debugsym;
