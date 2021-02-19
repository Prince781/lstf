#include "lstf-ir-node.h"
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>

void *lstf_ir_node_ref(void *node)
{
    if (!node)
        return NULL;
    lstf_ir_node *ir_node = node;
    assert(ir_node->floating || ir_node->refcount > 0);

    if (ir_node->floating) {
        ir_node->floating = false;
        ir_node->refcount = 1;
    } else {
        ir_node->refcount++;
    }

    return node;
}

void lstf_ir_node_unref(void *node) {
    if (!node)
        return;
    lstf_ir_node *ir_node = node;
    assert(ir_node->floating || ir_node->refcount > 0);

    if (ir_node->floating || --ir_node->refcount == 0) {
        // destroy
        ir_node->destructor(ir_node);
        free(ir_node);
    }
}

void lstf_ir_node_construct(lstf_ir_node     *node,
                            void            (*destructor)(lstf_ir_node *),
                            lstf_ir_node_type node_type)
{
    assert(destructor && "destructor required for LSTF IR node");

    node->node_type = node_type;
    node->refcount = 0;
    node->floating  = true;
    node->destructor = destructor;
}
