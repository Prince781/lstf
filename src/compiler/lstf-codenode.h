#pragma once

#include "lstf-sourceref.h"
#include <stdbool.h>

enum _lstf_codenode_type {
    lstf_codenode_type_statement,
    lstf_codenode_type_expression,
    lstf_codenode_type_symbol,
    lstf_codenode_type_block
};
typedef enum _lstf_codenode_type lstf_codenode_type;

typedef struct _lstf_codenode lstf_codenode;
typedef void (*lstf_codenode_dtor_func)(lstf_codenode *);

struct _lstf_codenode {
    lstf_codenode_type codenode_type;
    struct {
        unsigned refcount : sizeof(unsigned) * 8 - 1;
        bool floating : 1;
    };
    lstf_codenode_dtor_func dtor_func;
    lstf_sourceref source_reference;
    lstf_codenode *parent_node;
};

void *lstf_codenode_ref(void *node);

void lstf_codenode_unref(void *node);

void lstf_codenode_construct(lstf_codenode          *node, 
                             lstf_codenode_type      type,
                             const lstf_sourceref   *source_reference,
                             lstf_codenode_dtor_func dtor_func);

#define lstf_codenode_set_parent(node, parent) ((lstf_codenode *)(node))->parent_node = ((lstf_codenode *)(parent))
