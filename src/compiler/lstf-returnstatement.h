#pragma once

#include "lstf-sourceref.h"
#include "lstf-expression.h"
#include "lstf-statement.h"

struct _lstf_returnstatement {
    lstf_statement parent_struct;
    lstf_expression *expression;
};
typedef struct _lstf_returnstatement lstf_returnstatement;

lstf_statement *lstf_returnstatement_new(const lstf_sourceref *source_reference,
                                         lstf_expression      *expression);
