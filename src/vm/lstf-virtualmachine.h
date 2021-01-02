#pragma once

#include "data-structures/ptr-hashmap.h"
#include "lstf-vm-status.h"
#include "lstf-vm-stack.h"
#include "lstf-vm-program.h"
#include "lstf-vm-value.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct _lstf_virtualmachine {
    lstf_vm_program *program;   // the code and data of the program
    lstf_vm_stack *stack;       // the program's stack
    uint8_t *pc;                // program counter
    lstf_vm_status last_status; // status of the last-executed instruction
    int return_code;            // the return code (if the virtual machine has exited)
    ptr_hashmap *breakpoints;   // maps `(uint8_t *) -> (uint8_t *)`
    bool debug;                 // whether the virtual machine is in debug mode
    bool should_stop;           // whether the virtual machine should stop on the next iteration
};
/**
 * The virtual machine
 */
typedef struct _lstf_virtualmachine lstf_virtualmachine;

lstf_virtualmachine *lstf_virtualmachine_new(lstf_vm_program *program, bool debug);

void lstf_virtualmachine_destroy(lstf_virtualmachine *vm);

/**
 * Runs the virtual machine until termination or interruption.
 *
 * Returns `true` if interrupted and should continue, `false` otherwise.
 */
bool lstf_virtualmachine_run(lstf_virtualmachine *vm);

// --- debugging

/**
 * Sets a new breakpoint as an offset from the start of the code section.
 *
 * Returns a value `> 0` if successful (the breakpoint ID), `0` if the
 * breakpoint was not in range.
 */
unsigned lstf_virtualmachine_add_breakpoint(lstf_virtualmachine *vm, ptrdiff_t breakpoint);

bool lstf_virtualmachine_delete_breakpoint(lstf_virtualmachine *vm, unsigned id);
