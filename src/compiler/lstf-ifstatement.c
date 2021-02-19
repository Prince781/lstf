#include "lstf-ifstatement.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-statement.h"
#include <assert.h>
#include <stdlib.h>

static void lstf_ifstatement_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_if_statement(visitor, (lstf_ifstatement *)node);
}

static void lstf_ifstatement_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_ifstatement *stmt = (lstf_ifstatement *)node;

    lstf_codenode_accept(stmt->condition, visitor);
    lstf_codenode_accept(stmt->true_statements, visitor);
    if (stmt->false_statements)
        lstf_codenode_accept(stmt->false_statements, visitor);
}

static void lstf_ifstatement_destruct(lstf_codenode *node)
{
    lstf_ifstatement *stmt = (lstf_ifstatement *)node;

    lstf_codenode_unref(stmt->condition);
    lstf_codenode_unref(stmt->true_statements);
    lstf_codenode_unref(stmt->false_statements);
}

static const lstf_codenode_vtable lstf_ifstatement_vtable = {
    lstf_ifstatement_accept,
    lstf_ifstatement_accept_children,
    lstf_ifstatement_destruct
};

lstf_statement *lstf_ifstatement_new(const lstf_sourceref *source_reference,
                                     lstf_expression      *condition,
                                     lstf_block           *true_statements,
                                     lstf_block           *false_statements)
{
    assert(condition && true_statements && "if-statement must have condition and true statement");

    lstf_ifstatement *if_stmt = calloc(1, sizeof *if_stmt);

    lstf_statement_construct((lstf_statement *)if_stmt,
            &lstf_ifstatement_vtable,
            source_reference,
            lstf_statement_type_ifstatement);

    if_stmt->condition = lstf_codenode_ref(condition);
    lstf_codenode_set_parent(if_stmt->condition, if_stmt);
    if_stmt->true_statements = lstf_codenode_ref(true_statements);
    lstf_codenode_set_parent(if_stmt->true_statements, if_stmt);
    if (false_statements) {
        if_stmt->false_statements = lstf_codenode_ref(false_statements);
        lstf_codenode_set_parent(if_stmt->false_statements, if_stmt);
    }

    return (lstf_statement *)if_stmt;
}
