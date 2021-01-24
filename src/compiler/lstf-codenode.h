#pragma once

#include "lstf-common.h"
#include "lstf-sourceref.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

enum _lstf_codenode_type {
    lstf_codenode_type_statement,
    lstf_codenode_type_expression,
    lstf_codenode_type_symbol,
    lstf_codenode_type_block,
    lstf_codenode_type_scope,
    lstf_codenode_type_datatype
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
    unsigned refcount : sizeof(unsigned) * CHAR_BIT - 1;
    bool floating : 1;
    lstf_sourceref source_reference;
    lstf_codenode *parent_node;
};

static inline lstf_codenode *lstf_codenode_cast(void *node)
{
    if (!node)
        return NULL;

    lstf_codenode *code_node = node;
    switch (code_node->codenode_type) {
    case lstf_codenode_type_statement:
    case lstf_codenode_type_expression:
    case lstf_codenode_type_block:
    case lstf_codenode_type_scope:
    case lstf_codenode_type_symbol:
    case lstf_codenode_type_datatype:
        return code_node;
    }

    return NULL;
}

void *lstf_codenode_ref(void *node);

void lstf_codenode_unref(void *node);

static inline lstf_codenode *
lstf_codenode_set_parent(void *node, void *parent)
{
    return lstf_codenode_cast(node)->parent_node = lstf_codenode_cast(parent);
}

static inline void
lstf_codenode_set_source_reference(void *node, const lstf_sourceref *source_reference)
{
    lstf_codenode *code_node = lstf_codenode_cast(node);

    if (!source_reference)
        memset(&code_node->source_reference, 0, sizeof code_node->source_reference);
    else
        code_node->source_reference = *source_reference;
}

void lstf_codenode_construct(lstf_codenode              *node, 
                             const lstf_codenode_vtable *vtable,
                             lstf_codenode_type          type,
                             const lstf_sourceref       *source_reference)
    __attribute__((nonnull (1, 2)));
