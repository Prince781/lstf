#pragma once

#include "lstf-expression.h"
#include "lstf-statement.h"

struct _lstf_expressionstatement {
    lstf_statement parent_struct;
    lstf_expression *expression;
};
typedef struct _lstf_expressionstatement lstf_expressionstatement;

lstf_statement *lstf_expressionstatement_new(const lstf_sourceref *source_reference, 
                                             lstf_expression      *expression);
