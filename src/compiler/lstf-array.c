#include "lstf-array.h"
#include "compiler/lstf-codenode.h"
#include "compiler/lstf-expression.h"
#include "data-structures/collection.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include <stdint.h>
#include <stdlib.h>

static void lstf_array_destruct(lstf_codenode *code_node)
{
    lstf_array *array = (lstf_array *) code_node;

    ptr_list_destroy(array->expression_list);
    array->expression_list = NULL;
}

lstf_expression *lstf_array_new(const lstf_sourceref *source_reference, bool is_pattern)
{
    lstf_array *array = calloc(1, sizeof *array);

    lstf_expression_construct((lstf_expression *) array, 
            source_reference, lstf_array_destruct, lstf_expression_type_array);

    array->expression_list = ptr_list_new((collection_item_ref_func) lstf_codenode_ref, 
            (collection_item_unref_func) lstf_codenode_unref);
    array->is_pattern = is_pattern;

    return (lstf_expression *) array;
}

void lstf_array_add_element(lstf_array *array, lstf_expression *element)
{
    ptr_list_append(array->expression_list, element);
}

iterator lstf_array_iterator_create(lstf_array *array)
{
    return ptr_list_iterator_create(array->expression_list);
}
