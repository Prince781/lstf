#include "lstf-assignment.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-statement.h"
#include <stdlib.h>

static void lstf_assignment_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_assignment(visitor, (lstf_assignment *)code_node);
}

static void lstf_assignment_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_assignment *assign = (lstf_assignment *)code_node;

    lstf_codenode_accept(assign->lhs, visitor);
    lstf_codenode_accept(assign->rhs, visitor);
}

static void lstf_assignment_destruct(lstf_codenode *code_node)
{
    lstf_assignment *stmt = (lstf_assignment *)code_node;

    lstf_codenode_unref(stmt->lhs);
    lstf_codenode_unref(stmt->rhs);
}

static const lstf_codenode_vtable assignment_vtable = {
    lstf_assignment_accept,
    lstf_assignment_accept_children,
    lstf_assignment_destruct
};

lstf_statement *lstf_assignment_new(const lstf_sourceref *source_reference,
                                    lstf_expression      *lhs, 
                                    lstf_expression      *rhs)
{
    lstf_assignment *stmt = calloc(1, sizeof *stmt);

    lstf_statement_construct((lstf_statement *)stmt,
            &assignment_vtable,
            source_reference, 
            lstf_statement_type_assignment);

    stmt->lhs = lstf_codenode_ref(lhs);
    lstf_codenode_set_parent(lhs, stmt);
    stmt->rhs = lstf_codenode_ref(rhs);
    lstf_codenode_set_parent(rhs, stmt);

    return (lstf_statement *)stmt;
}
