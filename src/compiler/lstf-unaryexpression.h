#pragma once

#include "lstf-sourceref.h"
#include "lstf-expression.h"

enum _lstf_unaryoperator {
    lstf_unaryoperator_logical_not,
    lstf_unaryoperator_bitwise_not,
    lstf_unaryoperator_negate
};
typedef enum _lstf_unaryoperator lstf_unaryoperator;

struct _lstf_unaryexpression {
    lstf_expression parent_struct;
    lstf_unaryoperator op;
    lstf_expression *inner;
};
typedef struct _lstf_unaryexpression lstf_unaryexpression;

lstf_expression *lstf_unaryexpression_new(const lstf_sourceref *source_reference,
                                          lstf_unaryoperator    op,
                                          lstf_expression      *inner);
