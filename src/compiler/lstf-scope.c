#include "lstf-scope.h"
#include "lstf-codenode.h"
#include "lstf-symbol.h"
#include "data-structures/ptr-hashmap.h"
#include "util.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void lstf_scope_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{   // do nothing
    (void) node;
    (void) visitor;
}

static void lstf_scope_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{   // do nothing
    (void) node;
    (void) visitor;
}

static void lstf_scope_destruct(lstf_codenode *node)
{
    lstf_scope *scope = (lstf_scope *)node;

    ptr_hashmap_destroy(scope->symbol_table);
    scope->symbol_table = NULL;
    scope->owner = NULL;
}

static lstf_codenode_vtable scope_vtable = {
    lstf_scope_accept,
    lstf_scope_accept_children,
    lstf_scope_destruct
};

lstf_scope *lstf_scope_new(lstf_codenode *owner)
{
    lstf_scope *scope = calloc(1, sizeof *scope);

    assert((owner->codenode_type == lstf_codenode_type_block ||
                (owner->codenode_type == lstf_codenode_type_symbol &&
                 ((lstf_symbol *)owner)->symbol_type == lstf_symbol_type_function)) && 
            "scope owner must be block or function");

    lstf_codenode_construct((lstf_codenode *)scope, 
            &scope_vtable,
            lstf_codenode_type_scope,
            &owner->source_reference);
    
    scope->owner = owner;
    scope->symbol_table = ptr_hashmap_new((collection_item_hash_func) strhash, 
            (collection_item_ref_func) strdup, 
            free,
            strequal, 
            lstf_codenode_ref, 
            lstf_codenode_unref);

    return scope;
}

void lstf_scope_add_symbol(lstf_scope *scope, lstf_symbol *symbol)
{
    ptr_hashmap_insert(scope->symbol_table, symbol->name, symbol);
}

lstf_symbol *lstf_scope_get_symbol(lstf_scope *scope, const char *name)
{
    ptr_hashmap_entry *entry = ptr_hashmap_get(scope->symbol_table, name);

    return entry ? entry->value : NULL;
}
