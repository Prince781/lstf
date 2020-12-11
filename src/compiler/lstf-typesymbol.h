#pragma once

#include "lstf-scope.h"
#include "data-structures/iterator.h"
#include "lstf-codenode.h"
#include "lstf-sourceref.h"
#include "lstf-symbol.h"
#include "data-structures/ptr-hashmap.h"

enum _lstf_typesymbol_type {
    lstf_typesymbol_type_interface,
    lstf_typesymbol_type_enum,
    lstf_typesymbol_type_alias
};
typedef enum _lstf_typesymbol_type lstf_typesymbol_type;

/**
 * A statically-defined type symbol
 */
struct _lstf_typesymbol {
    lstf_symbol parent_struct;

    lstf_typesymbol_type typesymbol_type;

    /**
     * `(char *) -> (lstf_symbol *)`
     */
    ptr_hashmap *members;

    lstf_scope *scope;
};
typedef struct _lstf_typesymbol lstf_typesymbol;

static inline lstf_typesymbol *lstf_typesymbol_cast(void *node)
{
    lstf_codenode *code_node = node;

    if (code_node && code_node->codenode_type == lstf_codenode_type_symbol &&
            ((lstf_symbol *)code_node)->symbol_type == lstf_symbol_type_typesymbol)
        return (lstf_typesymbol *)code_node;
    return NULL;
}

void lstf_typesymbol_construct(lstf_typesymbol            *type_symbol,
                               const lstf_codenode_vtable *vtable,
                               const lstf_sourceref       *source_reference,
                               lstf_typesymbol_type        typesymbol_type,
                               char                       *name,
                               bool                        is_builtin)
    __attribute__((nonnull (1, 2, 5)));

void lstf_typesymbol_destruct(lstf_codenode *code_node);

void lstf_typesymbol_add_member(lstf_typesymbol *self, lstf_symbol *member);

lstf_symbol *lstf_typesymbol_get_member(lstf_typesymbol *self, const char *member_name);

lstf_symbol *lstf_typesymbol_lookup(lstf_typesymbol *self, const char *member_name);
