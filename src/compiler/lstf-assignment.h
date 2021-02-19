#pragma once

#include "lstf-sourceref.h"
#include "lstf-expression.h"
#include "lstf-variable.h"
#include "lstf-statement.h"
#include <stdbool.h>

struct _lstf_assignment {
    lstf_statement parent_struct;

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

static inline lstf_assignment *lstf_assignment_cast(void *node)
{
    lstf_statement *stmt = lstf_statement_cast(node);

    if (stmt && stmt->stmt_type == lstf_statement_type_assignment)
        return node;

    return NULL;
}

lstf_statement *lstf_assignment_new(const lstf_sourceref *source_reference,
                                    lstf_expression      *lhs,
                                    lstf_expression      *rhs)
    __attribute__((nonnull (2, 3)));
