#include "lstf-lambdaexpression.h"
#include "lstf-codevisitor.h"
#include "lstf-scope.h"
#include "lstf-codenode.h"
#include "lstf-expression.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include <stdio.h>
#include <stdlib.h>

static void lstf_lambdaexpression_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_lambda_expression(visitor, (lstf_lambdaexpression *)node);
    lstf_codevisitor_visit_expression(visitor, lstf_expression_cast(node));
}

static void lstf_lambdaexpression_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_lambdaexpression *expr = (lstf_lambdaexpression *)node;

    // visit all parameters
    for (iterator it = ptr_list_iterator_create(expr->parameters); it.has_next; it = iterator_next(it))
        lstf_codenode_accept(iterator_get_item(it), visitor);

    if (expr->expression_body)
        lstf_codenode_accept(expr->expression_body, visitor);
    else
        lstf_codenode_accept(expr->statements_body, visitor);
}

static void lstf_lambdaexpression_destruct(lstf_codenode *node)
{
    lstf_lambdaexpression *expr = (lstf_lambdaexpression *)node;

    lstf_codenode_unref(expr->scope);
    ptr_list_destroy(expr->parameters);
    lstf_codenode_unref(expr->expression_body);
    lstf_codenode_unref(expr->statements_body);

    lstf_expression_destruct(node);
}

static const lstf_codenode_vtable lambdaexpression_vtable = {
    lstf_lambdaexpression_accept,
    lstf_lambdaexpression_accept_children,
    lstf_lambdaexpression_destruct
};

static lstf_expression *lstf_lambdaexpression_new(const lstf_sourceref *source_reference,
                                                  lstf_expression      *expression_body,
                                                  lstf_block           *statements_body,
                                                  bool                  is_async)
{
    lstf_lambdaexpression *expr = calloc(1, sizeof *expr);

    if (!expr) {
        perror("failed to create lstf_lambdaexpression");
        abort();
    }

    lstf_expression_construct((lstf_expression *)expr,
            &lambdaexpression_vtable,
            source_reference,
            lstf_expression_type_lambda);

    expr->scope = lstf_codenode_ref(lstf_scope_new((lstf_codenode *)expr));
    expr->parameters = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);

    if (expression_body)
        expr->expression_body = lstf_codenode_ref(expression_body);
    else if (statements_body)
        expr->statements_body = lstf_codenode_ref(statements_body);

    expr->is_async = is_async;

    return (lstf_expression *)expr;
}

lstf_expression *
lstf_lambdaexpression_new_with_expression_body(const lstf_sourceref *source_reference,
                                               lstf_expression      *expression_body,
                                               bool                  is_async)
{
    return lstf_lambdaexpression_new(source_reference, expression_body, NULL, is_async);
}

lstf_expression *
lstf_lambdaexpression_new_with_statements_body(const lstf_sourceref *source_reference,
                                               lstf_block           *statements_body,
                                               bool                  is_async)
{
    return lstf_lambdaexpression_new(source_reference, NULL, statements_body, is_async);
}

void lstf_lambdaexpression_add_parameter(lstf_lambdaexpression *expr,
                                         lstf_variable         *parameter)
{
    ptr_list_append(expr->parameters, parameter);
    lstf_codenode_set_parent(parameter, expr);
}
