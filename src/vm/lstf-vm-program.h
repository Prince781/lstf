#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include "data-structures/ptr-hashmap.h"
#include "lstf-vm-debug.h"

static_assert(CHAR_BIT == 8, "8 bits per char required for program deserialization");
struct _lstf_vm_program {
    unsigned long refcount : sizeof(unsigned long)*CHAR_BIT - 1;
    bool floating : 1;

    // --- important sections
    uint8_t *file;                      // beginning point of the entire file (the memory-mapped file)

    // --- debug info fields (optional)
    char *source_file;
    /**
     * maps `(uint8_t *) -> (lstf_vm_debugentry *)`
     */
    ptr_hashmap *debug_entries;
    /**
     * maps `(uint8_t *) -> (lstf_vm_debugsym *)`
     */
    ptr_hashmap *debug_symbols;

    // --- data
    uint8_t *data;                      // beginning point of the data section
    uint64_t data_size;

    // --- code
    uint8_t *code;                      // beginning point of the code section
    uint8_t *entry_point;               // offset in `code` to begin execution
    uint64_t code_size;
};
typedef struct _lstf_vm_program lstf_vm_program;

/**
 * Loads a compiled LSTF program. The file must have the extension `.lstfc`
 */
lstf_vm_program *lstf_vm_program_load(const char *filename);

lstf_vm_program *lstf_vm_program_ref(lstf_vm_program *prog);

void lstf_vm_program_unref(lstf_vm_program *prog);
