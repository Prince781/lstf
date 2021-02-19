#include "lstf-binaryexpression.h"
#include "lstf-codenode.h"
#include "lstf-expression.h"
#include "lstf-codevisitor.h"
#include <stdio.h>
#include <stdlib.h>

static void lstf_binaryexpression_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_binary_expression(visitor, (lstf_binaryexpression *)code_node);
    lstf_codevisitor_visit_expression(visitor, lstf_expression_cast(code_node));
}

static void lstf_binaryexpression_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_binaryexpression *expr = (lstf_binaryexpression *)code_node;

    lstf_codenode_accept(expr->left, visitor);
    lstf_codenode_accept(expr->right, visitor);
}

static void lstf_binaryexpression_destruct(lstf_codenode *code_node)
{
    lstf_binaryexpression *expr = (lstf_binaryexpression *)code_node;

    lstf_codenode_unref(expr->left);
    lstf_codenode_unref(expr->right);
    lstf_expression_destruct(code_node);
}

static const lstf_codenode_vtable binaryexpression_vtable = {
    lstf_binaryexpression_accept,
    lstf_binaryexpression_accept_children,
    lstf_binaryexpression_destruct
};

lstf_expression *lstf_binaryexpression_new(const lstf_sourceref *source_reference,
                                           lstf_binaryoperator   op,
                                           lstf_expression      *left,
                                           lstf_expression      *right)
{
    lstf_binaryexpression *expr = calloc(1, sizeof *expr);

    if (!expr) {
        perror("failed to create lstf_binaryexpression");
        abort();
    }

    lstf_expression_construct((lstf_expression *)expr,
            &binaryexpression_vtable,
            source_reference,
            lstf_expression_type_binary);

    expr->op = op;
    expr->left = lstf_codenode_ref(left);
    lstf_codenode_set_parent(expr->left, expr);
    expr->right = lstf_codenode_ref(right);
    lstf_codenode_set_parent(expr->right, expr);

    return (lstf_expression *)expr;
}
