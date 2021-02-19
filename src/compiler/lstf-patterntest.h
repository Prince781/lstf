#pragma once

#include "compiler/lstf-scope.h"
#include "data-structures/ptr-hashset.h"
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

    /**
     * List of captured variables. This is needed because pattern test
     * statements may be outlined when they are asynchronous.
     */
    ptr_hashset *captured_locals;

    /**
     * Needed when statements may be outlined.
     */
    lstf_scope *hidden_scope;

    /**
     * Whether this expression contains calls to asynchronous functions that
     * are not awaited.
     */
    bool is_async;

    /**
     * The unique identifier for this pattern test
     */
    unsigned id;
};
typedef struct _lstf_patterntest lstf_patterntest;

static inline lstf_patterntest *
lstf_patterntest_cast(void *node)
{
    lstf_statement *stmt = lstf_statement_cast(node);
    if (stmt && stmt->stmt_type == lstf_statement_type_patterntest)
        return (lstf_patterntest *)stmt;
    return NULL;
}

lstf_statement *lstf_patterntest_new(const lstf_sourceref     *source_reference,
                                     lstf_expression          *pattern,
                                     lstf_expression          *expression);

void lstf_patterntest_add_captured_local(lstf_patterntest *stmt,
                                         lstf_symbol      *var_or_fn);
