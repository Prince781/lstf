#include "lstf-expressionstatement.h"
#include "lstf-codenode.h"
#include "lstf-statement.h"
#include <stdlib.h>

void lstf_expressionstatement_destruct(lstf_codenode *code_node)
{
    lstf_expressionstatement *stmt = (lstf_expressionstatement *)code_node;

    lstf_codenode_unref(stmt->expression);
}

lstf_statement *lstf_expressionstatement_new(const lstf_sourceref *source_reference, 
                                             lstf_expression      *expression)
{
    lstf_expressionstatement *stmt = calloc(1, sizeof *stmt);

    lstf_statement_construct((lstf_statement *) stmt, 
            source_reference,
            lstf_expressionstatement_destruct,
            lstf_statement_type_expression);

    stmt->expression = lstf_codenode_ref(expression);
    lstf_codenode_set_parent(stmt->expression, stmt);

    return (lstf_statement *)stmt;
}
