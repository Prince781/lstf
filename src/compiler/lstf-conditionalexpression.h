#pragma once

#include "lstf-sourceref.h"
#include "lstf-expression.h"

/**
 * AKA ternary expression:
 * `<test> ? <expression> : <expression>`
 */
struct _lstf_conditionalexpression {
    lstf_expression parent_struct;
    lstf_expression *condition;
    lstf_expression *true_expression;
    lstf_expression *false_expression;
};
typedef struct _lstf_conditionalexpression lstf_conditionalexpression;

lstf_expression *lstf_conditionalexpression_new(const lstf_sourceref *source_reference,
                                                lstf_expression      *condition,
                                                lstf_expression      *true_expression,
                                                lstf_expression      *false_expression)
    __attribute__((nonnull(2, 3, 4)));
