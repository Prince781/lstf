#pragma once

#include "lstf-file.h"
#include "lstf-codenode.h"
#include "lstf-symbol.h"
#include "data-structures/ptr-hashmap.h"

struct _lstf_scope {
    lstf_codenode parent_struct;

    /**
     * Maps `(char *) -> ((weak ref) lstf_symbol *)`
     */
    ptr_hashmap *symbol_table;
};
typedef struct _lstf_scope lstf_scope;

lstf_scope *lstf_scope_new(lstf_codenode *owner);

void lstf_scope_add_symbol(lstf_scope *scope, lstf_symbol *symbol);

/**
 * Search for symbol named [name] in just this scope
 */
lstf_symbol *lstf_scope_get_symbol(lstf_scope *scope, const char *name);

/**
 * Search for symbol named [name] in this scope and all parent scopes
 */
lstf_symbol *lstf_scope_lookup(lstf_scope *scope, const char *name);
