#include "lstf-patterntest.h"
#include "compiler/lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-statement.h"
#include <stdio.h>
#include <stdlib.h>

static void lstf_patterntest_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_patterntest *stmt = (lstf_patterntest *)code_node;

    lstf_codevisitor_visit_pattern_test(visitor, stmt);
}

static void lstf_patterntest_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_patterntest *stmt = (lstf_patterntest *)code_node;

    lstf_codenode_accept(stmt->pattern, visitor);
    lstf_codenode_accept(stmt->expression, visitor);
}

static void lstf_patterntest_destruct(lstf_codenode *code_node)
{
    lstf_patterntest *stmt = (lstf_patterntest *)code_node;

    lstf_codenode_unref(stmt->pattern);
    stmt->pattern = NULL;
    lstf_codenode_unref(stmt->expression);
    stmt->expression = NULL;
}

static const lstf_codenode_vtable patterntest_vtable = {
    lstf_patterntest_accept,
    lstf_patterntest_accept_children,
    lstf_patterntest_destruct
};

lstf_statement *lstf_patterntest_new(const lstf_sourceref     *source_reference,
                                     lstf_expression          *pattern,
                                     lstf_expression          *expression)
{
    lstf_patterntest *stmt = calloc(1, sizeof *stmt);

    if (!stmt) {
        perror("failed to create lstf_patterntest");
        abort();
    }

    lstf_statement_construct((lstf_statement *)stmt,
            &patterntest_vtable,
            source_reference,
            lstf_statement_type_patterntest);

    stmt->pattern = lstf_codenode_ref(pattern);
    stmt->expression = lstf_codenode_ref(expression);

    return (lstf_statement *) stmt;
}
