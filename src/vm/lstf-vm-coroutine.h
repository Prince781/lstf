#pragma once

#include "data-structures/ptr-list.h"
#include "lstf-vm-stack.h"
#include <stdbool.h>
#include <limits.h>
#include <stdint.h>

struct _lstf_vm_coroutine {
    unsigned refcount : sizeof(unsigned) * CHAR_BIT - 1;
    bool floating : 1;
    unsigned outstanding_io;            // number of I/O tasks this coroutine is waiting on
    lstf_vm_stack *stack;               // the coroutine's stack
    uint8_t *pc;                        // program counter
    ptr_list_node *node;                // reference to node in run queue/suspend list
};
typedef struct _lstf_vm_coroutine lstf_vm_coroutine;

/**
 * Create a coroutine starting at [pc]
 */
lstf_vm_coroutine *lstf_vm_coroutine_new(uint8_t *pc);

lstf_vm_coroutine *lstf_vm_coroutine_ref(lstf_vm_coroutine *cr);

void lstf_vm_coroutine_unref(lstf_vm_coroutine *cr);
