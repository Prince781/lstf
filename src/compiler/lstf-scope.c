#include "lstf-scope.h"
#include "lstf-expression.h"
#include "lstf-function.h"
#include "lstf-lambdaexpression.h"
#include "lstf-typesymbol.h"
#include "lstf-codenode.h"
#include "lstf-symbol.h"
#include "lstf-block.h"
#include "data-structures/ptr-hashmap.h"
#include "util.h"
#include <assert.h>
#include <stdio.h>
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
}

static lstf_codenode_vtable scope_vtable = {
    lstf_scope_accept,
    lstf_scope_accept_children,
    lstf_scope_destruct
};

lstf_scope *lstf_scope_new(lstf_codenode *owner)
{
    assert((owner->codenode_type == lstf_codenode_type_block ||
                (owner->codenode_type == lstf_codenode_type_symbol &&
                 (((lstf_symbol *)owner)->symbol_type == lstf_symbol_type_function ||
                  ((lstf_symbol *)owner)->symbol_type == lstf_symbol_type_typesymbol)) ||
                (owner->codenode_type == lstf_codenode_type_expression &&
                 ((lstf_expression *)owner)->expr_type == lstf_expression_type_lambda)) && 
            "scope owner must be block, function, type symbol, lambda expression");

    lstf_scope *scope = calloc(1, sizeof *scope);

    if (!scope) {
        perror("failed to create lstf_scope");
        abort();
    }

    lstf_codenode_construct((lstf_codenode *)scope, 
            &scope_vtable,
            lstf_codenode_type_scope,
            &owner->source_reference);
    
    lstf_codenode_set_parent(scope, owner);
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
    const ptr_hashmap_entry *entry = ptr_hashmap_get(scope->symbol_table, name);

    return entry ? entry->value : NULL;
}

lstf_symbol *lstf_scope_lookup(lstf_scope *scope, const char *name)
{
    lstf_symbol *found_symbol = lstf_scope_get_symbol(scope, name);

    if (found_symbol)
        return found_symbol;

    lstf_codenode *owner = ((lstf_codenode *)scope)->parent_node;
    lstf_codenode *owner_parent_node = owner->parent_node;

    while (owner_parent_node) {
        lstf_block *owner_parent_block = lstf_block_cast(owner_parent_node);
        if (owner_parent_block)
            return lstf_scope_lookup(owner_parent_block->scope, name);

        lstf_function *owner_parent_function = lstf_function_cast(owner_parent_node);
        if (owner_parent_function)
            return lstf_scope_lookup(owner_parent_function->scope, name);

        lstf_typesymbol *owner_parent_typesymbol = lstf_typesymbol_cast(owner_parent_node);
        if (owner_parent_typesymbol)
            return lstf_scope_lookup(owner_parent_typesymbol->scope, name);

        lstf_lambdaexpression *owner_parent_lambda = lstf_lambdaexpression_cast(owner_parent_node);
        if (owner_parent_lambda)
            return lstf_scope_lookup(owner_parent_lambda->scope, name);

        owner_parent_node = owner_parent_node->parent_node;
    }

    return NULL;
}
