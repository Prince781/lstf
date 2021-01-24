#include "lstf-expression.h"
#include "lstf-datatype.h"
#include "lstf-codenode.h"
#include <stdlib.h>
#include <stdio.h>

void lstf_expression_construct(lstf_expression            *expr,
                               const lstf_codenode_vtable *vtable,
                               const lstf_sourceref       *source_reference,
                               lstf_expression_type        expr_type)
{
    lstf_codenode_construct((lstf_codenode *)expr,
            vtable,
            lstf_codenode_type_expression, 
            source_reference);
    expr->expr_type = expr_type;
}

void lstf_expression_destruct(lstf_codenode *node)
{
    lstf_codenode_unref(((lstf_expression *)node)->value_type);
}

void lstf_expression_set_value_type(lstf_expression *expression, lstf_datatype *data_type)
{
    lstf_codenode_unref(expression->value_type);

    if (((lstf_codenode *)data_type)->parent_node)
        data_type = lstf_datatype_copy(data_type);
    expression->value_type = lstf_codenode_ref(data_type);

    lstf_codenode_set_parent(expression->value_type, expression);
}
