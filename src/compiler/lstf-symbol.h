#pragma once

#include "lstf-sourceref.h"
#include "lstf-codenode.h"
#include <stddef.h>

enum _lstf_symbol_type {
    lstf_symbol_type_variable,
    lstf_symbol_type_objectproperty,
    lstf_symbol_type_function,
    lstf_symbol_type_typesymbol
};
typedef enum _lstf_symbol_type lstf_symbol_type;

struct _lstf_symbol {
    lstf_codenode parent_struct;
    lstf_symbol_type symbol_type;
    char *name;
};
typedef struct _lstf_symbol lstf_symbol;

static inline lstf_symbol *lstf_symbol_cast(void *node)
{
    lstf_codenode *code_node = node;

    if (code_node && code_node->codenode_type == lstf_codenode_type_symbol)
        return (lstf_symbol *)code_node;
    return NULL;
}

void lstf_symbol_construct(lstf_symbol                *symbol,
                           const lstf_codenode_vtable *vtable,
                           const lstf_sourceref       *source_reference,
                           lstf_symbol_type            symbol_type,
                           char                       *name);

void lstf_symbol_destruct(lstf_codenode *code_node);
