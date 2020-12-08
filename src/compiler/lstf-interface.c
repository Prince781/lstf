#include "lstf-interface.h"
#include "data-structures/string-builder.h"
#include "lstf-datatype.h"
#include "lstf-interfacetype.h"
#include "lstf-symbol.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include "lstf-typesymbol.h"
#include "lstf-sourceref.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static void lstf_interfaceproperty_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_interface_property(visitor, (lstf_interfaceproperty *)node);
}

static void lstf_interfaceproperty_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_interfaceproperty *property = (lstf_interfaceproperty *)node;
    lstf_codenode_accept(property->property_type, visitor);
}

static void lstf_interfaceproperty_destruct(lstf_codenode *node)
{
    lstf_interfaceproperty *property = (lstf_interfaceproperty *)node;

    lstf_codenode_unref(property->property_type);
    property->property_type = NULL;
    lstf_symbol_destruct(node);
}

static const lstf_codenode_vtable interfaceproperty_vtable = {
    lstf_interfaceproperty_accept,
    lstf_interfaceproperty_accept_children,
    lstf_interfaceproperty_destruct
};

lstf_interfaceproperty *lstf_interfaceproperty_new(const lstf_sourceref *source_reference,
                                                   const char           *name,
                                                   bool                  is_optional,
                                                   lstf_datatype        *property_type,
                                                   bool                  is_builtin)
{
    lstf_interfaceproperty *property = calloc(1, sizeof *property);

    lstf_symbol_construct((lstf_symbol *)property,
            &interfaceproperty_vtable,
            source_reference,
            lstf_symbol_type_interfaceproperty,
            strdup(name),
            is_builtin);

    property->is_optional = is_optional;
    if (lstf_codenode_cast(property_type)->parent_node)
        property_type = lstf_datatype_copy(property_type);
    property->property_type = lstf_codenode_ref(property_type);
    lstf_codenode_set_parent(property_type, property);

    return property;
}

void lstf_interfaceproperty_set_property_type(lstf_interfaceproperty *property,
                                              lstf_datatype          *property_type)
{
    if (((lstf_codenode *)property_type)->parent_node)
        property_type = lstf_datatype_copy(property_type);
    lstf_codenode_unref(property->property_type);
    property->property_type = lstf_codenode_ref(property_type);
    lstf_codenode_set_parent(property_type, property);
}

static void lstf_interface_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_interface(visitor, (lstf_interface *)code_node);
}

static void lstf_interface_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_interface *interface = (lstf_interface *)code_node;

    for (iterator it = ptr_list_iterator_create(interface->extends_types); it.has_next; it = iterator_next(it))
        lstf_codenode_accept(iterator_get_item(it), visitor);

    for (iterator it = ptr_hashmap_iterator_create(((lstf_typesymbol *)interface)->members); 
            it.has_next; it = iterator_next(it)) {
        const ptr_hashmap_entry *entry = iterator_get_item(it);
        lstf_codenode_accept(entry->value, visitor);
    }
}

static void lstf_interface_destruct(lstf_codenode *code_node)
{
    lstf_interface *interface = (lstf_interface *)code_node;

    ptr_list_destroy(interface->extends_types);
    interface->extends_types = NULL;

    lstf_typesymbol_destruct(code_node);
}

static const lstf_codenode_vtable interface_vtable = {
    lstf_interface_accept,
    lstf_interface_accept_children,
    lstf_interface_destruct
};

static unsigned num_interfaces_created = 0;

static char *lstf_interface_anonymous_name_new(void)
{
    string *representation = string_new();

    string_appendf(representation, "<anonymous interface #%u>", num_interfaces_created + 1); 
    return string_destroy(representation);
}

lstf_interface *lstf_interface_new(const lstf_sourceref *source_reference,
                                   const char           *name,
                                   bool                  is_anonymous,
                                   bool                  is_builtin)
{
    assert(!name == is_anonymous);
    lstf_interface *interface = calloc(1, sizeof *interface);

    lstf_typesymbol_construct((lstf_typesymbol *)interface,
            &interface_vtable,
            source_reference,
            lstf_typesymbol_type_interface,
            is_anonymous ? lstf_interface_anonymous_name_new() : strdup(name),
            is_builtin);

    interface->extends_types = ptr_list_new((collection_item_ref_func) lstf_codenode_ref, 
            (collection_item_unref_func) lstf_codenode_unref);
    interface->is_anonymous = is_anonymous;
    num_interfaces_created++;

    return interface;
}

void lstf_interface_add_base_type(lstf_interface *interface, lstf_datatype *base_type)
{
    ptr_list_append(interface->extends_types, base_type);
}

void lstf_interface_add_member(lstf_interface *interface, lstf_interfaceproperty *property)
{
    lstf_typesymbol_add_member(lstf_typesymbol_cast(interface), lstf_symbol_cast(property));
}

void lstf_interface_replace_base_type(lstf_interface *interface,
                                      lstf_datatype  *old_base_type,
                                      lstf_datatype  *new_base_type)
{
    ptr_list_node *found_node = ptr_list_replace(interface->extends_types, old_base_type, NULL, new_base_type);

    assert(found_node && "attempting to replace a non-existent base type!");
}
