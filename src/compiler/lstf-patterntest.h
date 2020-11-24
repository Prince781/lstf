#pragma once

#include "lstf-codenode.h"
#include "lstf-sourceref.h"
#include "lstf-statement.h"
#include "lstf-expression.h"

struct _lstf_patterntest {
    lstf_statement parent_struct;

    /**
     * Pattern on the left-hand side
     */
    lstf_expression *pattern;

    /**
     * Expression on the right-hand side
     */
    lstf_expression *expression;
};
typedef struct _lstf_patterntest lstf_patterntest;

lstf_statement *lstf_patterntest_new(const lstf_sourceref     *source_reference,
                                     lstf_expression          *pattern,
                                     lstf_expression          *expression);
