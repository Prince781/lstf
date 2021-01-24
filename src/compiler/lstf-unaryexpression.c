#include "lstf-unaryexpression.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-expression.h"
#include <stdio.h>
#include <stdlib.h>

static void lstf_unaryexpression_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_unary_expression(visitor, (lstf_unaryexpression *)code_node);
    lstf_codevisitor_visit_expression(visitor, lstf_expression_cast(code_node));
}

static void lstf_unaryexpression_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_unaryexpression *expr = (lstf_unaryexpression *)code_node;

    lstf_codenode_accept(expr->inner, visitor);
}

static void lstf_unaryexpression_destruct(lstf_codenode *code_node)
{
    lstf_unaryexpression *expr = (lstf_unaryexpression *)code_node;

    lstf_codenode_unref(expr->inner);
    lstf_expression_destruct(code_node);
}

static const lstf_codenode_vtable unaryexpression_vtable = {
    lstf_unaryexpression_accept,
    lstf_unaryexpression_accept_children,
    lstf_unaryexpression_destruct
};

lstf_expression *lstf_unaryexpression_new(const lstf_sourceref *source_reference,
                                          lstf_unaryoperator    op,
                                          lstf_expression      *inner)
{
    lstf_unaryexpression *expr = calloc(1, sizeof *expr);

    if (!expr) {
        perror("failed to create lstf_unaryexpression");
        abort();
    }

    lstf_expression_construct((lstf_expression *)expr,
            &unaryexpression_vtable,
            source_reference,
            lstf_expression_type_unary);

    expr->op = op;
    expr->inner = lstf_codenode_ref(inner);

    return (lstf_expression *)expr;
}
