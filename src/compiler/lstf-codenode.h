#pragma once

#include "lstf-sourceref.h"
#include <stdbool.h>
#include <stddef.h>

enum _lstf_codenode_type {
    lstf_codenode_type_statement,
    lstf_codenode_type_expression,
    lstf_codenode_type_symbol,
    lstf_codenode_type_block,
    lstf_codenode_type_scope
};
typedef enum _lstf_codenode_type lstf_codenode_type;

typedef struct _lstf_codenode lstf_codenode;

typedef struct _lstf_codevisitor lstf_codevisitor;
struct _lstf_codenode_vtable {
    /**
     * Accepts a code visitor for this node
     */
    void (*accept)(lstf_codenode *self, lstf_codevisitor *visitor);

    /**
     * Iterates over all children of this code node and calls lstf_codenode_accept()
     */
    void (*accept_children)(lstf_codenode *self, lstf_codevisitor *visitor);

    /**
     * Cleanup function to run when before code node is free()'d
     */
    void (*destructor)(lstf_codenode *self);
};
typedef struct _lstf_codenode_vtable lstf_codenode_vtable;

/**
 * Accepts a code visitor for this node
 */
void lstf_codenode_accept(void *node, lstf_codevisitor *visitor);

/**
 * Iterates over all children of this code node and calls `lstf_codenode_accept()`
 */
void lstf_codenode_accept_children(void *node, lstf_codevisitor *visitor);

struct _lstf_codenode {
    const lstf_codenode_vtable *codenode_vtable;
    lstf_codenode_type codenode_type;
    struct {
        unsigned refcount : sizeof(unsigned) * 8 - 1;
        bool floating : 1;
    };
    lstf_sourceref source_reference;
    lstf_codenode *parent_node;
};

static inline lstf_codenode *lstf_codenode_cast(void *node)
{
    lstf_codenode *code_node = node;

    if (!code_node)
        return NULL;

    switch (code_node->codenode_type) {
    case lstf_codenode_type_statement:
    case lstf_codenode_type_expression:
    case lstf_codenode_type_block:
    case lstf_codenode_type_scope:
    case lstf_codenode_type_symbol:
        return code_node;
    }

    return NULL;
}

void *lstf_codenode_ref(void *node);

void lstf_codenode_unref(void *node);

void lstf_codenode_construct(lstf_codenode              *node, 
                             const lstf_codenode_vtable *vtable,
                             lstf_codenode_type          type,
                             const lstf_sourceref       *source_reference);

#define lstf_codenode_set_parent(node, parent) ((lstf_codenode *)(node))->parent_node = ((lstf_codenode *)(parent))
