#include "lstf-statement.h"
#include "lstf-codenode.h"

void lstf_statement_construct(lstf_statement             *stmt,
                              const lstf_codenode_vtable *vtable,
                              const lstf_sourceref       *source_reference,
                              lstf_statement_type         stmt_type)
{
    lstf_codenode_construct((lstf_codenode *) stmt, 
            vtable,
            lstf_codenode_type_statement, 
            source_reference);
    stmt->stmt_type = stmt_type;
}
