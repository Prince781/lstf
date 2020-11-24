#pragma once

#include "lstf-codenode.h"

enum _lstf_statement_type {
    lstf_statement_type_assignment,
    lstf_statement_type_patterntest,

    /**
     * An expression statement.
     */
    lstf_statement_type_expression,
};
typedef enum _lstf_statement_type lstf_statement_type;

struct _lstf_statement {
    lstf_codenode parent_struct;
    lstf_statement_type stmt_type;
};
typedef struct _lstf_statement lstf_statement;

void lstf_statement_construct(lstf_statement            *stmt,
                              const lstf_sourceref      *source_reference,
                              lstf_codenode_dtor_func    dtor_func,
                              lstf_statement_type        stmt_type);
