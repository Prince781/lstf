#pragma once

#include "lstf-sourceref.h"
#include "lstf-codenode.h"

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

void lstf_symbol_construct(lstf_symbol              *symbol,
                           const lstf_sourceref     *source_reference,
                           lstf_codenode_dtor_func   dtor_func,
                           lstf_symbol_type          symbol_type,
                           char                     *name);

void lstf_symbol_destruct(lstf_codenode *code_node);
