#include "lstf-object.h"
#include "data-structures/iterator.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-sourceref.h"
#include "lstf-symbol.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include "lstf-expression.h"
#include <string.h>
#include <stdlib.h>

static void lstf_objectproperty_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_object_property(visitor, (lstf_objectproperty *)code_node);
}

static void lstf_objectproperty_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_objectproperty *property = (lstf_objectproperty *)code_node;

    lstf_codenode_accept(property->value, visitor);
}

static void lstf_objectproperty_destruct(lstf_codenode *code_node)
{
    lstf_objectproperty *prop = (lstf_objectproperty *)code_node;
    lstf_codenode_unref(prop->value);
    lstf_symbol_destruct(code_node);
}

static const lstf_codenode_vtable objectproperty_vtable = {
    lstf_objectproperty_accept,
    lstf_objectproperty_accept_children,
    lstf_objectproperty_destruct
};

lstf_objectproperty *
lstf_objectproperty_new(const lstf_sourceref *source_reference,
                        const char           *name,
                        bool                  is_nullable,
                        lstf_expression      *value)
{
    lstf_objectproperty *prop = calloc(1, sizeof *prop);

    lstf_symbol_construct((lstf_symbol *) prop,
            &objectproperty_vtable,
            source_reference,
            lstf_symbol_type_objectproperty,
            strdup(name));

    prop->is_nullable = is_nullable;
    prop->value = lstf_codenode_ref(value);
    lstf_codenode_set_parent(prop->value, prop);

    return prop;
}

static void lstf_object_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_object(visitor, (lstf_object *)code_node);
    lstf_codevisitor_visit_expression(visitor, lstf_expression_cast(code_node));
}

static void lstf_object_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_object *object = (lstf_object *)code_node;

    for (iterator it = lstf_object_iterator_create(object); it.has_next; it = iterator_next(it))
        lstf_codenode_accept(iterator_get_item(it), visitor);
}

static void lstf_object_destruct(lstf_codenode *code_node)
{
    lstf_object *object = (lstf_object *) code_node;

    ptr_list_destroy(object->members_list);
    object->members_list = NULL;
}

static const lstf_codenode_vtable object_vtable = {
    lstf_object_accept,
    lstf_object_accept_children,
    lstf_object_destruct
};

lstf_object *lstf_object_new(const lstf_sourceref *source_reference, bool is_pattern)
{
    lstf_object *object = calloc(1, sizeof *object);

    lstf_expression_construct((lstf_expression *) object, 
            &object_vtable,
            source_reference, 
            lstf_expression_type_object);

    object->members_list = ptr_list_new((collection_item_ref_func) lstf_codenode_ref, 
            (collection_item_unref_func) lstf_codenode_unref);
    object->is_pattern = is_pattern;

    return object;
}

void lstf_object_add_property(lstf_object *object, lstf_objectproperty *property)
{
    ptr_list_append(object->members_list, property);
    lstf_codenode_set_parent(property, object);
}

iterator lstf_object_iterator_create(lstf_object *object)
{
    return ptr_list_iterator_create(object->members_list);
}
