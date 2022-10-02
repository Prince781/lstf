#pragma once

#include "lstf-virtualmachine.h"
#include "lstf-vm-status.h"

/**
 * Routines accessible with the `vmcall` instruction.
 */
extern lstf_vm_status (*const vmcall_table[256])(lstf_virtualmachine *, lstf_vm_coroutine *);
