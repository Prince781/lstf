#include "lstf-vm-value.h"
#include "lstf-vm-coroutine.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

lstf_vm_upvalue *lstf_vm_upvalue_new(int64_t stack_offset, lstf_vm_coroutine *cr)
{
    lstf_vm_upvalue *upvalue = calloc(1, sizeof *upvalue);

    if (!upvalue) {
        perror("failed to create up-value");
        abort();
    }

    upvalue->floating = true;
    upvalue->is_local = true;
    upvalue->stack_offset = stack_offset;
    upvalue->cr = cr;

    return upvalue;
}

lstf_vm_upvalue *lstf_vm_upvalue_ref(lstf_vm_upvalue *upvalue)
{
    if (!upvalue)
        return NULL;

    assert(upvalue->floating || upvalue->refcount > 0);

    if (upvalue->floating) {
        upvalue->floating = false;
        upvalue->refcount = 1;
    } else {
        upvalue->refcount++;
    }

    return upvalue;
}

void lstf_vm_upvalue_unref(lstf_vm_upvalue *upvalue)
{
    if (!upvalue)
        return;

    assert(upvalue->floating || upvalue->refcount > 0);

    if (upvalue->floating || --upvalue->refcount == 0) {
        if (!upvalue->is_local) {
            lstf_vm_value_clear(&upvalue->value);
        } else {
            // if this up-value is being destroyed, then the stack location is
            // no longer captured
            assert(upvalue->stack_offset < upvalue->cr->stack->n_values);
            assert(upvalue->cr->stack->values[upvalue->stack_offset].is_captured &&
                    "is there more than one up-value for the same stack value?");
            // we can do this because there should be only one up-value that
            // aliases a stack value
            upvalue->cr->stack->values[upvalue->stack_offset].is_captured = false;
        }
        free(upvalue);
    }
}

lstf_vm_closure *lstf_vm_closure_new(uint8_t         *code_address,
                                     uint8_t          num_upvalues,
                                     lstf_vm_upvalue *upvalues[])
{
    lstf_vm_closure *closure = calloc(1, offsetof(lstf_vm_closure, upvalues) + num_upvalues * sizeof(lstf_vm_upvalue *));

    if (!closure) {
        perror("failed to create VM closure");
        abort();
    }

    closure->floating = true;
    closure->code_address = code_address;
    closure->num_upvalues = num_upvalues;
    for (unsigned i = 0; i < num_upvalues; i++)
        closure->upvalues[i] = lstf_vm_upvalue_ref(upvalues[i]);

    return closure;
}

lstf_vm_closure *lstf_vm_closure_ref(lstf_vm_closure *closure)
{
    if (!closure)
        return NULL;

    assert(closure->floating || closure->refcount > 0);

    if (closure->floating) {
        closure->floating = false;
        closure->refcount = 1;
    } else {
        closure->refcount++;
    }

    return closure;
}

void lstf_vm_closure_unref(lstf_vm_closure *closure)
{
    if (!closure)
        return;

    assert(closure->floating || closure->refcount > 0);

    if (closure->floating || --closure->refcount == 0) {
        for (unsigned i = 0; i < closure->num_upvalues; i++)
            lstf_vm_upvalue_unref(closure->upvalues[i]);
        free(closure);
    }
}
