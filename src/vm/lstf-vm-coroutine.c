#include "lstf-vm-coroutine.h"
#include "lstf-vm-stack.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

lstf_vm_coroutine *lstf_vm_coroutine_new(uint8_t *pc)
{
    lstf_vm_coroutine *cr = calloc(1, sizeof *cr);

    if (!cr) {
        perror("failed to create LSTF VM coroutine");
        abort();
    }

    cr->floating = true;
    cr->pc = pc;
    cr->stack = lstf_vm_stack_new();

    return cr;
}

lstf_vm_coroutine *lstf_vm_coroutine_ref(lstf_vm_coroutine *cr)
{
    if (!cr)
        return NULL;

    if (cr->floating) {
        cr->floating = false;
        cr->refcount = 1;
    } else {
        cr->refcount++;
    }

    return cr;
}

void lstf_vm_coroutine_unref(lstf_vm_coroutine *cr)
{
    if (!cr)
        return;
    assert(cr->floating || cr->refcount > 0);

    if (cr->floating || --cr->refcount == 0) {
        // destroy coroutine
        lstf_vm_stack_destroy(cr->stack);
        free(cr);
    }
}
