#pragma once

#include "lstf-sourceref.h"
#include "lstf-expression.h"
#include "lstf-variable.h"
#include "lstf-statement.h"
#include <stdbool.h>

struct _lstf_assignment {
    lstf_statement parent_struct;

    bool is_declaration;

    /**
     * Left-hand side of the assignment
     */
    lstf_expression *lhs;

    /**
     * The right-hand side expression
     */
    lstf_expression *rhs;
};
typedef struct _lstf_assignment lstf_assignment;

lstf_statement *lstf_assignment_new(const lstf_sourceref *source_reference,
                                    bool                  is_declaration,
                                    lstf_expression      *lhs,
                                    lstf_expression      *rhs);
