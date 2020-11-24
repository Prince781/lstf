#include "lstf-patterntest.h"
#include "compiler/lstf-codenode.h"
#include "lstf-statement.h"
#include <stdlib.h>

static void lstf_patterntest_destruct(lstf_codenode *code_node)
{
    lstf_patterntest *stmt = (lstf_patterntest *)code_node;

    lstf_codenode_unref(stmt->pattern);
    stmt->pattern = NULL;
    lstf_codenode_unref(stmt->expression);
    stmt->expression = NULL;
}

lstf_statement *lstf_patterntest_new(const lstf_sourceref     *source_reference,
                                     lstf_expression          *pattern,
                                     lstf_expression          *expression)
{
    lstf_patterntest *stmt = calloc(1, sizeof *stmt);

    lstf_statement_construct((lstf_statement *)stmt,
            source_reference,
            lstf_patterntest_destruct,
            lstf_statement_type_patterntest);

    stmt->pattern = lstf_codenode_ref(pattern);
    stmt->expression = lstf_codenode_ref(expression);

    return (lstf_statement *) stmt;
}
