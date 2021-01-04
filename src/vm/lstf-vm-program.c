#include "lstf-vm-program.h"
#include "data-structures/ptr-hashmap.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

lstf_vm_program *lstf_vm_program_ref(lstf_vm_program *prog)
{
    if (!prog)
        return NULL;

    assert(prog->floating || prog->refcount > 0);

    if (prog->floating) {
        prog->floating = false;
        prog->refcount = 1;
    } else {
        prog->refcount++;
    }

    return prog;
}

static void lstf_vm_program_destroy(lstf_vm_program *prog)
{
    if (!prog)
        return;

    assert(prog->floating || prog->refcount == 0);

    free(prog->debug);
    ptr_hashmap_destroy(prog->debug_entries);
    ptr_hashmap_destroy(prog->debug_symbols);
    free(prog->data);
    free(prog->code);
    memset(prog, 0, sizeof *prog);

    free(prog);
}

void lstf_vm_program_unref(lstf_vm_program *prog)
{
    if (!prog)
        return;

    assert(prog->floating || prog->refcount > 0);

    if (prog->floating || --prog->refcount == 0)
        lstf_vm_program_destroy(prog);
}
