#include "lstf-assignment.h"
#include "compiler/lstf-codenode.h"
#include "compiler/lstf-statement.h"
#include <stdlib.h>

void lstf_assignment_destruct(lstf_codenode *code_node)
{
    lstf_assignment *stmt = (lstf_assignment *)code_node;

    lstf_codenode_unref(stmt->variable);
    lstf_codenode_unref(stmt->expression);
}

lstf_statement *lstf_assignment_new(const lstf_sourceref *source_reference,
                                    bool                  is_declaration, 
                                    lstf_variable        *variable, 
                                    lstf_expression      *expression)
{
    lstf_assignment *stmt = calloc(1, sizeof *stmt);

    lstf_statement_construct((lstf_statement *)stmt,
            source_reference, 
            lstf_assignment_destruct,
            lstf_statement_type_assignment);

    stmt->is_declaration = is_declaration;
    stmt->variable = lstf_codenode_ref(variable);
    stmt->expression = lstf_codenode_ref(expression);

    return (lstf_statement *)stmt;
}
