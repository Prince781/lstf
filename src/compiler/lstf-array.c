#include "lstf-array.h"
#include "compiler/lstf-codenode.h"
#include "compiler/lstf-codevisitor.h"
#include "compiler/lstf-expression.h"
#include "data-structures/collection.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void lstf_array_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_array(visitor, (lstf_array *)code_node);
    lstf_codevisitor_visit_expression(visitor, lstf_expression_cast(code_node));
}

static void lstf_array_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_array *array = (lstf_array *)code_node;

    for (iterator it = lstf_array_iterator_create(array); it.has_next; it = iterator_next(it))
        lstf_codenode_accept(iterator_get_item(it), visitor);
}

static void lstf_array_destruct(lstf_codenode *code_node)
{
    lstf_array *array = (lstf_array *) code_node;

    ptr_list_destroy(array->expression_list);
    array->expression_list = NULL;
    lstf_expression_destruct(code_node);
}

static const lstf_codenode_vtable array_vtable = {
    lstf_array_accept,
    lstf_array_accept_children,
    lstf_array_destruct
};

lstf_expression *lstf_array_new(const lstf_sourceref *source_reference, bool is_pattern)
{
    lstf_array *array = calloc(1, sizeof *array);

    if (!array) {
        perror("failed to create lstf_array");
        abort();
    }

    lstf_expression_construct((lstf_expression *) array, 
            &array_vtable, 
            source_reference, 
            lstf_expression_type_array);

    array->expression_list = ptr_list_new((collection_item_ref_func) lstf_codenode_ref, 
            (collection_item_unref_func) lstf_codenode_unref);
    array->is_pattern = is_pattern;

    return (lstf_expression *) array;
}

void lstf_array_add_element(lstf_array *array, lstf_expression *element)
{
    ptr_list_append(array->expression_list, element);
    lstf_codenode_set_parent(element, array);
}

iterator lstf_array_iterator_create(lstf_array *array)
{
    return ptr_list_iterator_create(array->expression_list);
}
