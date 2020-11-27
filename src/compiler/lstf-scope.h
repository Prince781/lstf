#pragma once

#include "lstf-file.h"
#include "lstf-codenode.h"
#include "lstf-symbol.h"
#include "data-structures/ptr-hashmap.h"

struct _lstf_scope {
    lstf_codenode parent_struct;

    /**
     * (weak ref) The symbol or block that opens up this scope
     */
    lstf_codenode *owner;

    /**
     * Maps `(char *) -> ((weak ref) lstf_symbol *)`
     */
    ptr_hashmap *symbol_table;
};
typedef struct _lstf_scope lstf_scope;

lstf_scope *lstf_scope_new(lstf_codenode *owner);

void lstf_scope_add_symbol(lstf_scope *scope, lstf_symbol *symbol);

lstf_symbol *lstf_scope_get_symbol(lstf_scope *scope, const char *name);
