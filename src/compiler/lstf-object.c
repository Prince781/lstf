#include "lstf-object.h"
#include "compiler/lstf-codenode.h"
#include "compiler/lstf-sourceref.h"
#include "compiler/lstf-symbol.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include "lstf-expression.h"
#include <stdlib.h>

void lstf_objectproperty_destruct(lstf_codenode *code_node)
{
    lstf_objectproperty *prop = (lstf_objectproperty *)code_node;
    lstf_codenode_unref(prop->value);
    lstf_symbol_destruct(code_node);
}

lstf_objectproperty *
lstf_objectproperty_new(const lstf_sourceref *source_reference,
                        char                 *name,
                        bool                  is_nullable,
                        lstf_expression      *value)
{
    lstf_objectproperty *prop = calloc(1, sizeof *prop);

    lstf_symbol_construct((lstf_symbol *) prop,
            source_reference,
            lstf_objectproperty_destruct,
            lstf_symbol_type_objectproperty,
            name);

    prop->is_nullable = is_nullable;
    prop->value = lstf_codenode_ref(value);
    lstf_codenode_set_parent(prop->value, prop);

    return prop;
}


static void
lstf_object_destruct(lstf_codenode *code_node)
{
    lstf_object *object = (lstf_object *) code_node;

    ptr_list_destroy(object->members_list);
    object->members_list = NULL;
}

lstf_object *lstf_object_new(const lstf_sourceref *source_reference, bool is_pattern)
{
    lstf_object *object = calloc(1, sizeof *object);

    lstf_expression_construct((lstf_expression *) object, 
            source_reference, 
            lstf_object_destruct,
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
