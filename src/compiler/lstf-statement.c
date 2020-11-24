#include "lstf-statement.h"
#include "lstf-codenode.h"

void lstf_statement_construct(lstf_statement            *stmt,
                              const lstf_sourceref      *source_reference,
                              lstf_codenode_dtor_func    dtor_func,
                              lstf_statement_type        stmt_type)
{
    lstf_codenode_construct((lstf_codenode *) stmt, 
        lstf_codenode_type_statement, 
        source_reference, 
        dtor_func);
    stmt->stmt_type = stmt_type;
}
