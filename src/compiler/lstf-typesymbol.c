#include "lstf-typesymbol.h"
#include "compiler/lstf-interfacetype.h"
#include "data-structures/ptr-list.h"
#include "lstf-interface.h"
#include "lstf-codevisitor.h"
#include "lstf-scope.h"
#include "data-structures/iterator.h"
#include "lstf-codenode.h"
#include "data-structures/ptr-hashmap.h"
#include "lstf-sourceref.h"
#include "lstf-symbol.h"
#include "util.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

void lstf_typesymbol_destruct(lstf_codenode *code_node)
{
    lstf_typesymbol *type_symbol = (lstf_typesymbol *)code_node;

    lstf_codenode_unref(type_symbol->scope);
    ptr_hashmap_destroy(type_symbol->members);
    type_symbol->members = NULL;
    lstf_symbol_destruct(code_node);
}

void lstf_typesymbol_construct(lstf_typesymbol            *type_symbol,
                               const lstf_codenode_vtable *vtable,
                               const lstf_sourceref       *source_reference,
                               lstf_typesymbol_type        typesymbol_type,
                               char                       *name,
                               bool                        is_builtin)
{
    lstf_symbol_construct((lstf_symbol *)type_symbol,
            vtable,
            source_reference,
            lstf_symbol_type_typesymbol,
            name,
            is_builtin);

    type_symbol->typesymbol_type = typesymbol_type;
    type_symbol->scope = lstf_codenode_ref(lstf_scope_new((lstf_codenode *)type_symbol));
    type_symbol->members = ptr_hashmap_new((collection_item_hash_func) strhash,
            (collection_item_ref_func) strdup,
            free,
            strequal,
            lstf_codenode_ref,
            lstf_codenode_unref);
}

void lstf_typesymbol_add_member(lstf_typesymbol *self, lstf_symbol *member)
{
    assert(!lstf_codenode_cast(member)->parent_node &&
            "adding a member that belongs to another symbol!");
    assert(self->typesymbol_type != lstf_typesymbol_type_alias &&
            "adding a member to an alias type!");

    ptr_hashmap_insert(self->members, member->name, member);
    lstf_scope_add_symbol(self->scope, member);
    lstf_codenode_set_parent(member, self);
}

lstf_symbol *lstf_typesymbol_get_member(lstf_typesymbol *self, const char *member_name)
{
    const ptr_hashmap_entry *entry = ptr_hashmap_get(self->members, member_name);

    return entry ? entry->value : NULL;
}

lstf_symbol *lstf_typesymbol_lookup(lstf_typesymbol *self, const char *member_name)
{
    lstf_symbol *member = lstf_typesymbol_get_member(self, member_name);

    if (!member && self->typesymbol_type == lstf_typesymbol_type_interface) {
        lstf_interface *interface = lstf_interface_cast(self);
        for (iterator it = ptr_list_iterator_create(interface->extends_types); it.has_next; it = iterator_next(it)) {
            lstf_datatype *base_type = iterator_get_item(it);

            if (base_type->datatype_type == lstf_datatype_type_interfacetype &&
                    (member = lstf_typesymbol_lookup(lstf_typesymbol_cast(base_type->symbol), member_name)))
                break;
        }
    }

    return member;
}
