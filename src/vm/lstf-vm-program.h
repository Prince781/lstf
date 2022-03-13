#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include "data-structures/ptr-hashmap.h"
#include "lstf-vm-debug.h"
#include "io/outputstream.h"

static_assert(CHAR_BIT == 8, "8 bits per char required for program deserialization");
struct _lstf_vm_program {
    unsigned long refcount : sizeof(unsigned long)*CHAR_BIT - 1;
    bool floating : 1;

    // --- important sections
    // --- debug info fields (optional)
    uint8_t *debuginfo;                 // mapped debug section (optional)
    uint64_t debuginfo_size;
    char *source_filename;
    /**
     * maps `(uint8_t *) -> (lstf_vm_debugentry *)`
     */
    ptr_hashmap *debug_entries;
    /**
     * maps `(uint8_t *) -> (lstf_vm_debugsym *)`
     */
    ptr_hashmap *debug_symbols;

    // --- data
    uint8_t *data;                      // mapped data section
    uint64_t data_size;

    // --- code
    uint8_t *code;                      // mapped code section
    uint8_t *entry_point;               // offset in `code` to begin execution
    uint64_t code_size;
};
typedef struct _lstf_vm_program lstf_vm_program;

lstf_vm_program *lstf_vm_program_ref(lstf_vm_program *prog);

/**
 * Dereferences the program. The program will be unloaded when its reference
 * count hits 0.
 */
void lstf_vm_program_unref(lstf_vm_program *prog);

/**
 * Disassemble the program to plain text.
 *
 * @return whether we were able to write the output
 */
bool lstf_vm_program_disassemble(lstf_vm_program *prog, outputstream *ostream);
