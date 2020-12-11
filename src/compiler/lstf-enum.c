#include "lstf-enum.h"
#include "lstf-codevisitor.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "lstf-codenode.h"
#include "lstf-symbol.h"
#include "lstf-typesymbol.h"
#include <string.h>
#include <stdlib.h>

static void lstf_enum_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_enum(visitor, (lstf_enum *)node);
}

static void lstf_enum_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    for (iterator it = ptr_hashmap_iterator_create(((lstf_typesymbol *)node)->members);
            it.has_next; it = iterator_next(it)) {
        const ptr_hashmap_entry *entry = iterator_get_item(it);
        lstf_codenode_accept(entry->value, visitor);
    }
}

static void lstf_enum_destruct(lstf_codenode *node)
{
    lstf_typesymbol_destruct(node);
}

static const lstf_codenode_vtable enum_vtable = {
    lstf_enum_accept,
    lstf_enum_accept_children,
    lstf_enum_destruct
};

lstf_symbol *lstf_enum_new(const lstf_sourceref *source_reference,
                           const char           *name,
                           bool                  is_builtin)
{
    lstf_enum *enum_symbol = calloc(1, sizeof *enum_symbol);

    lstf_typesymbol_construct((lstf_typesymbol *)enum_symbol,
            &enum_vtable,
            source_reference,
            lstf_typesymbol_type_enum,
            strdup(name),
            is_builtin);

    return (lstf_symbol *)enum_symbol;
}

void lstf_enum_add_member(lstf_enum *self, lstf_constant *member)
{
    ptr_hashmap_insert(((lstf_typesymbol *)self)->members, lstf_symbol_cast(member)->name, member);
    lstf_scope_add_symbol(((lstf_typesymbol *)self)->scope, lstf_symbol_cast(member));
}

void lstf_enum_set_members_type(lstf_enum *self, lstf_datatype *data_type)
{
    lstf_codenode_unref(self->members_type);

    if (((lstf_codenode *)data_type)->parent_node)
        data_type = lstf_datatype_copy(data_type);

    self->members_type = lstf_codenode_ref(data_type);
    lstf_codenode_set_parent(self->members_type, self);
}
