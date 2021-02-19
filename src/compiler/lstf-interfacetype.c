#include "lstf-interfacetype.h"
#include "data-structures/string-builder.h"
#include "lstf-codevisitor.h"
#include "lstf-interface.h"
#include "lstf-symbol.h"
#include "lstf-typesymbol.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include "lstf-uniontype.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void lstf_interfacetype_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)node);
}

static void lstf_interfacetype_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_datatype *self = (lstf_datatype *)node;

    if (lstf_interface_cast(self->symbol)->is_anonymous)
        lstf_codenode_accept(self->symbol, visitor);
}

static void lstf_interfacetype_destruct(lstf_codenode *node)
{
    lstf_interfacetype *iface_type = (lstf_interfacetype *)node;

    if (iface_type->anonymous_interface)
        lstf_codenode_unref(iface_type->anonymous_interface);

    lstf_datatype_set_symbol(iface_type, NULL);
    lstf_datatype_destruct(lstf_datatype_cast(node));
}

static const lstf_codenode_vtable interfacetype_vtable = {
    lstf_interfacetype_accept,
    lstf_interfacetype_accept_children,
    lstf_interfacetype_destruct
};

static bool lstf_interfacetype_is_supertype_of(lstf_datatype *self, lstf_datatype *other)
{
    // check that all ancestors of this interface are also
    // supertypes of the other interface
    for (iterator it = ptr_list_iterator_create(lstf_interface_cast(self->symbol)->extends_types);
            it.has_next; it = iterator_next(it)) {
        lstf_datatype *prereq_type = iterator_get_item(it);
        if (!lstf_datatype_is_supertype_of(prereq_type, other))
            return false;
    }

    if (other->datatype_type == lstf_datatype_type_interfacetype) {
        // check that all members of this interface are contained 
        // in the other interface
        for (iterator it = ptr_hashmap_iterator_create(lstf_typesymbol_cast(self->symbol)->members);
                it.has_next; it = iterator_next(it)) {
            const ptr_hashmap_entry *entry = iterator_get_item(it);
            const char *member_name = entry->key;
            lstf_symbol *other_member = NULL;

            if (!(other_member = lstf_typesymbol_lookup(lstf_typesymbol_cast(other->symbol), member_name)))
                return false;

            lstf_interfaceproperty *prop = entry->value;
            lstf_interfaceproperty *other_prop = (lstf_interfaceproperty *) other_member;

            if (!lstf_datatype_is_supertype_of(prop->property_type, other_prop->property_type))
                return false;
        }

        return true;
    }

    if (other->datatype_type == lstf_datatype_type_uniontype) {
        for (iterator it = ptr_list_iterator_create(lstf_uniontype_cast(other)->options); it.has_next; it = iterator_next(it)) {
            if (!lstf_datatype_is_supertype_of(self, iterator_get_item(it)))
                return false;
        }
        return true;
    }

    return false;
}

static lstf_datatype *lstf_interfacetype_copy(lstf_datatype *self)
{
    return lstf_interfacetype_new(&((lstf_codenode *)self)->source_reference, 
            lstf_interface_cast(self->symbol));
}

static char *lstf_interfacetype_to_string(lstf_datatype *self_dt)
{
    string *representation = string_new();
    string_appendf(representation, "{");
    for (iterator it = ptr_hashmap_iterator_create(lstf_typesymbol_cast(self_dt->symbol)->members);
            it.has_next; it = iterator_next(it)) {
        const ptr_hashmap_entry *entry = iterator_get_item(it);
        const char *property_name = entry->key;
        lstf_interfaceproperty *property = entry->value;
        char *property_type_representation = lstf_datatype_to_string(property->property_type);

        string_appendf(representation, "%s %s%s: %s",
                it.is_first ? "" : ";",
                property_name,
                property->is_optional ? "?" : "",
                property_type_representation);

        free(property_type_representation);
    }
    if (!ptr_hashmap_is_empty(lstf_typesymbol_cast(self_dt->symbol)->members))
        string_appendf(representation, " "); 
    string_appendf(representation, "}");

    return string_destroy(representation);
}

static const lstf_datatype_vtable interfacetype_datatype_vtable = {
    lstf_interfacetype_is_supertype_of,
    lstf_interfacetype_copy,
    lstf_interfacetype_to_string
};

lstf_datatype *lstf_interfacetype_new(const lstf_sourceref *source_reference,
                                      lstf_interface       *interface)
{
    lstf_interfacetype *iface_type = calloc(1, sizeof *iface_type);

    if (!iface_type) {
        perror("failed to create lstf_interfacetype");
        abort();
    }

    lstf_datatype_construct((lstf_datatype *)iface_type,
            &interfacetype_vtable,
            source_reference,
            lstf_datatype_type_interfacetype,
            &interfacetype_datatype_vtable);

    // keep a weak reference to the interface, since the interface type could
    // be used by a member of the interface
    lstf_datatype_set_symbol(iface_type, interface);
    if (interface->is_anonymous)
        iface_type->anonymous_interface = lstf_codenode_ref(interface);

    return (lstf_datatype *)iface_type;
}
