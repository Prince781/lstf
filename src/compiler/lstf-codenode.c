#include "lstf-codenode.h"
#include <assert.h>
#include <stdlib.h>

void *lstf_codenode_ref(void *node)
{
    if (node) {
        lstf_codenode *code_node = node;
        if (code_node->floating) {
            code_node->floating = false;
            code_node->refcount = 1;
        } else
            code_node->refcount++;
    }

    return node;
}

static void lstf_codenode_destroy(lstf_codenode *node)
{
    if (!node)
        return;
    assert(node->floating || node->refcount == 0);
    node->dtor_func(node);
    free(node);
}

void lstf_codenode_unref(void *node)
{
    if (!node)
        return;
    lstf_codenode *code_node = node;
    assert(code_node->floating || code_node->refcount > 0);
    if (code_node->floating || --code_node->refcount == 0)
        lstf_codenode_destroy(code_node);
}

void lstf_codenode_construct(lstf_codenode          *node, 
                             lstf_codenode_type      type,
                             const lstf_sourceref   *source_reference,
                             lstf_codenode_dtor_func dtor_func)
{
    assert(dtor_func && "base class destructor must be provided");
    node->codenode_type = type;
    if (source_reference)
        node->source_reference = *source_reference;
    node->floating = true;
    node->dtor_func = dtor_func;
}
