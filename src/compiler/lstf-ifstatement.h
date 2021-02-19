#pragma once

#include "lstf-sourceref.h"
#include "lstf-block.h"
#include "lstf-expression.h"
#include "lstf-statement.h"

struct _lstf_ifstatement {
    lstf_statement parent_struct;
    lstf_expression *condition;
    lstf_block *true_statements;

    /**
     * May be NULL
     */
    lstf_block *false_statements;
};
typedef struct _lstf_ifstatement lstf_ifstatement;

lstf_statement *lstf_ifstatement_new(const lstf_sourceref *source_reference,
                                     lstf_expression      *condition,
                                     lstf_block           *true_statements,
                                     lstf_block           *false_statements);
