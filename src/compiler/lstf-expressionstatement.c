#include "lstf-expressionstatement.h"
#include "compiler/lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-statement.h"
#include <stdio.h>
#include <stdlib.h>

static void lstf_expressionstatement_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_expression_statement(visitor, (lstf_expressionstatement *)node);
}

static void lstf_expressionstatement_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_expressionstatement *stmt = (lstf_expressionstatement *)node;

    lstf_codenode_accept(stmt->expression, visitor);
}

static void lstf_expressionstatement_destruct(lstf_codenode *code_node)
{
    lstf_expressionstatement *stmt = (lstf_expressionstatement *)code_node;

    lstf_codenode_unref(stmt->expression);
}

static const lstf_codenode_vtable expressionstatement_vtable = {
    lstf_expressionstatement_accept,
    lstf_expressionstatement_accept_children,
    lstf_expressionstatement_destruct
};

lstf_statement *lstf_expressionstatement_new(const lstf_sourceref *source_reference, 
                                             lstf_expression      *expression)
{
    lstf_expressionstatement *stmt = calloc(1, sizeof *stmt);

    if (!stmt) {
        perror("failed to create lstf_expressionstatement");
        abort();
    }

    lstf_statement_construct((lstf_statement *) stmt, 
            &expressionstatement_vtable,
            source_reference,
            lstf_statement_type_expression);

    stmt->expression = lstf_codenode_ref(expression);
    lstf_codenode_set_parent(stmt->expression, stmt);

    return (lstf_statement *)stmt;
}
