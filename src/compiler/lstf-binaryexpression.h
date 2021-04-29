#pragma once

#include "lstf-sourceref.h"
#include "lstf-expression.h"

enum _lstf_binaryoperator {
    lstf_binaryoperator_in,
    lstf_binaryoperator_plus,
    lstf_binaryoperator_minus,
    lstf_binaryoperator_multiply,
    lstf_binaryoperator_divide,
    lstf_binaryoperator_modulo,
    lstf_binaryoperator_exponent,
    lstf_binaryoperator_coalescer,
    lstf_binaryoperator_leftshift,
    lstf_binaryoperator_rightshift,
    lstf_binaryoperator_lessthan,
    lstf_binaryoperator_lessthan_equal,
    lstf_binaryoperator_greaterthan,
    lstf_binaryoperator_greaterthan_equal,
    lstf_binaryoperator_equal,
    lstf_binaryoperator_notequal,
    lstf_binaryoperator_equivalent,
    lstf_binaryoperator_logical_or,
    lstf_binaryoperator_logical_and,
    lstf_binaryoperator_bitwise_or,
    lstf_binaryoperator_bitwise_and,
    lstf_binaryoperator_bitwise_xor,
};
typedef enum _lstf_binaryoperator lstf_binaryoperator;

struct _lstf_binaryexpression {
    lstf_expression parent_struct;
    lstf_binaryoperator op;
    lstf_expression *left;
    lstf_expression *right;
};
typedef struct _lstf_binaryexpression lstf_binaryexpression;

lstf_expression *lstf_binaryexpression_new(const lstf_sourceref *source_reference,
                                           lstf_binaryoperator   op,
                                           lstf_expression      *left,
                                           lstf_expression      *right);
