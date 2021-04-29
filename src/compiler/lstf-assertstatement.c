#include "lstf-assertstatement.h"
#include "compiler/lstf-codevisitor.h"
#include "lstf-statement.h"
#include "lstf-codenode.h"
#include <stdlib.h>

static void lstf_assertstatement_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_assert_statement(visitor, (lstf_assertstatement *)code_node);
}

static void lstf_assertstatement_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_assertstatement *stmt = (lstf_assertstatement *)code_node;
    lstf_codenode_accept(stmt->expression, visitor);
}

static void lstf_assertstatement_destruct(lstf_codenode *code_node)
{
    lstf_assertstatement *stmt = (lstf_assertstatement *)code_node;

    lstf_codenode_unref(stmt->expression);
}

static const lstf_codenode_vtable assertstatement_vtable = {
    lstf_assertstatement_accept,
    lstf_assertstatement_accept_children,
    lstf_assertstatement_destruct
};

lstf_statement *lstf_assertstatement_new(const lstf_sourceref *source_reference,
                                         lstf_expression      *expression)
{
    lstf_assertstatement *stmt = calloc(1, sizeof *stmt);

    lstf_statement_construct((lstf_statement *)stmt,
            &assertstatement_vtable,
            source_reference,
            lstf_statement_type_assert);

    stmt->expression = lstf_codenode_ref(expression);
    lstf_codenode_set_parent(stmt->expression, stmt);

    return (lstf_statement *)stmt;
}
