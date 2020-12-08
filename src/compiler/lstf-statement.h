#pragma once

#include "lstf-codenode.h"

enum _lstf_statement_type {
    lstf_statement_type_assignment,
    lstf_statement_type_patterntest,

    /**
     * An expression statement.
     */
    lstf_statement_type_expression,

    /**
     * A declaration for a type or function.
     */
    lstf_statement_type_declaration,

    /**
     * A return statement
     */
    lstf_statement_type_return,
};
typedef enum _lstf_statement_type lstf_statement_type;

struct _lstf_statement {
    lstf_codenode parent_struct;
    lstf_statement_type stmt_type;
};
typedef struct _lstf_statement lstf_statement;

void lstf_statement_construct(lstf_statement             *stmt,
                              const lstf_codenode_vtable *vtable,
                              const lstf_sourceref       *source_reference,
                              lstf_statement_type         stmt_type);
