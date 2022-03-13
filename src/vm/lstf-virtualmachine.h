#pragma once

#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include "io/event.h"
#include "io/outputstream.h"
#include "lstf-vm-status.h"
#include "lstf-vm-stack.h"
#include "lstf-vm-program.h"
#include "lstf-vm-value.h"
#include "lstf-vm-coroutine.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct _lstf_virtualmachine {
    lstf_vm_program *program;           // the code and data of the program
    lstf_vm_status last_status;         // status of the last-executed instruction
    int return_code;                    // the return code (if the virtual machine has exited)
    outputstream *ostream;              // the output stream for the virtual machine
    lstf_vm_coroutine *main_coroutine;  // the main coroutine
    uint8_t *last_pc;                   // PC of the last executing coroutine (for debugging)
    ptr_list *run_queue;                // queue of ready coroutines
    ptr_list *suspended_list;           // list of suspended coroutines
    ptr_hashmap *up_values;             // maps `lstf_vm_upvalue *` -> `lstf_vm_upvalue *`
    eventloop *event_loop;              // I/O event loop for all asynchronous operations
    unsigned instructions_executed;     // number of instructions executed since last context switch
    bool debug;                         // whether the virtual machine is in debug mode
    bool should_stop;                   // whether the virtual machine should stop on the next iteration
};
typedef struct _lstf_virtualmachine lstf_virtualmachine;

/**
 * Creates a new virtual machine.
 *
 * @param ostream can be `NULL`, in which case the default output stream will be used (stdout)
 */
lstf_virtualmachine *lstf_virtualmachine_new(lstf_vm_program *program,
                                             outputstream    *ostream,
                                             bool             debug);

void lstf_virtualmachine_destroy(lstf_virtualmachine *vm);

/**
 * Runs the virtual machine until termination or interruption.
 *
 * Returns `true` if interrupted but should continue, `false` if should not continue.
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
