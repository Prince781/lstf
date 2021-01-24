#include "lstf-conditionalexpression.h"
#include "lstf-codenode.h"
#include "lstf-expression.h"
#include "lstf-codevisitor.h"
#include <stdio.h>
#include <stdlib.h>

static void lstf_conditionalexpression_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_conditional_expression(visitor, (lstf_conditionalexpression *)node);
    lstf_codevisitor_visit_expression(visitor, lstf_expression_cast(node));
}

static void lstf_conditionalexpression_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_conditionalexpression *expr = (lstf_conditionalexpression *)node;

    lstf_codenode_accept(expr->condition, visitor);
    lstf_codenode_accept(expr->true_expression, visitor);
    lstf_codenode_accept(expr->false_expression, visitor);
}

static void lstf_conditionalexpression_destruct(lstf_codenode *node)
{
    lstf_conditionalexpression *expr = (lstf_conditionalexpression *)node;

    lstf_codenode_unref(expr->condition);
    lstf_codenode_unref(expr->true_expression);
    lstf_codenode_unref(expr->false_expression);

    lstf_expression_destruct(node);
}

static const lstf_codenode_vtable conditionalexpression_vtable = {
    lstf_conditionalexpression_accept,
    lstf_conditionalexpression_accept_children,
    lstf_conditionalexpression_destruct
};

lstf_expression *lstf_conditionalexpression_new(const lstf_sourceref *source_reference,
                                                lstf_expression      *condition,
                                                lstf_expression      *true_expression,
                                                lstf_expression      *false_expression)
{
    lstf_conditionalexpression *expr = calloc(1, sizeof *expr);

    if (!expr) {
        perror("failed to create lstf_conditionalexpression");
        abort();
    }

    lstf_expression_construct((lstf_expression *)expr,
            &conditionalexpression_vtable,
            source_reference,
            lstf_expression_type_conditional);

    expr->condition = lstf_codenode_ref(condition);
    expr->true_expression = lstf_codenode_ref(true_expression);
    expr->false_expression = lstf_codenode_ref(false_expression);

    return (lstf_expression *)expr;
}
