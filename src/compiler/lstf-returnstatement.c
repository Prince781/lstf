#include "lstf-returnstatement.h"
#include "lstf-codenode.h"
#include "lstf-statement.h"
#include "lstf-codevisitor.h"
#include <stdio.h>
#include <stdlib.h>

static void lstf_returnstatement_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_return_statement(visitor, (lstf_returnstatement *)node);
}

static void lstf_returnstatement_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_returnstatement *stmt = (lstf_returnstatement *)node;
    if (stmt->expression)
        lstf_codenode_accept(stmt->expression, visitor);
}

static void lstf_returnstatement_destruct(lstf_codenode *node)
{
    lstf_returnstatement *stmt = (lstf_returnstatement *)node;
    lstf_codenode_unref(stmt->expression);
    stmt->expression = NULL;
}

static const lstf_codenode_vtable returnstatement_vtable = {
    lstf_returnstatement_accept,
    lstf_returnstatement_accept_children,
    lstf_returnstatement_destruct
};

lstf_statement *lstf_returnstatement_new(const lstf_sourceref *source_reference,
                                         lstf_expression      *expression)
{
    lstf_returnstatement *return_stmt = calloc(1, sizeof *return_stmt);

    if (!return_stmt) {
        perror("failed to create lstf_returnstatement");
        abort();
    }

    lstf_statement_construct((lstf_statement *)return_stmt,
            &returnstatement_vtable,
            source_reference,
            lstf_statement_type_return);

    if (expression) {
        return_stmt->expression = lstf_codenode_ref(expression);
        lstf_codenode_set_parent(return_stmt->expression, return_stmt);
    }

    return (lstf_statement *)return_stmt;
}
