#pragma once

#include "lstf-codenode.h"
#include "lstf-sourceref.h"
#include "data-structures/iterator.h"
#include "lstf-expression.h"
#include "data-structures/ptr-list.h"

struct _lstf_array {
    lstf_expression parent_struct;
    ptr_list *expression_list;
    bool is_pattern;
};
typedef struct _lstf_array lstf_array;

static inline lstf_array *lstf_array_cast(void *node)
{
    lstf_codenode *code_node = node;

    if (code_node && code_node->codenode_type == lstf_codenode_type_expression &&
            ((lstf_expression *)code_node)->expr_type == lstf_expression_type_array)
        return (lstf_array *)code_node;
    return NULL;
}

lstf_expression *lstf_array_new(const lstf_sourceref *source_reference, bool is_pattern);

void lstf_array_add_element(lstf_array *array, lstf_expression *element);

iterator lstf_array_iterator_create(lstf_array *array);
