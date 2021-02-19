#pragma once

#include "data-structures/ptr-hashset.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include "data-structures/ptr-list.h"
#include "lstf-sourceref.h"
#include "lstf-variable.h"
#include "lstf-file.h"
#include "lstf-function.h"
#include "lstf-expression.h"
#include "lstf-block.h"

struct _lstf_lambdaexpression {
    lstf_expression parent_struct;

    /**
     * The scope introduced by the lambda expression, before the body.
     * Contains the parameters
     */
    lstf_scope *scope;

    /**
     * A list of `(lstf_variable *)`
     */
    ptr_list *parameters;

    /**
     * The expression that is returned, if applicable.
     *
     * For example, if the lambda was declared as `() => ...`
     *
     * Either this is non-NULL or [statements_body] is non-NULL.
     */
    lstf_expression *expression_body;

    /**
     * The block containing one or more statements, if applicable.
     *
     * For example, if the lambda was declared as `() => { ... }`
     *
     * Either this is non-NULL or [expression_body] is non-NULL
     */
    lstf_block *statements_body;

    /**
     * The local variables captured by this lambda.
     *
     * hash set of `(lstf_variable *)`
     */
    ptr_hashset *captured_locals;

    /**
     * The unique identifier for this lambda expression
     */
    unsigned id;

    /**
     * Whether this is an asychronous function
     */
    bool is_async;
};
typedef struct _lstf_lambdaexpression lstf_lambdaexpression;

static inline lstf_lambdaexpression *lstf_lambdaexpression_cast(void *node)
{
    lstf_expression *expression = lstf_expression_cast(node);

    if (expression && expression->expr_type == lstf_expression_type_lambda)
        return node;

    return NULL;
}

lstf_expression *
lstf_lambdaexpression_new_with_expression_body(const lstf_sourceref *source_reference,
                                               lstf_expression      *expression_body,
                                               bool                  is_async)
    __attribute__((nonnull(2)));

lstf_expression *
lstf_lambdaexpression_new_with_statements_body(const lstf_sourceref *source_reference,
                                               lstf_block           *statements_body,
                                               bool                  is_async)
    __attribute__((nonnull(2)));

void lstf_lambdaexpression_add_parameter(lstf_lambdaexpression *expr,
                                         lstf_variable         *parameter);

void lstf_lambdaexpression_add_captured_local(lstf_lambdaexpression *expr,
                                              lstf_symbol           *var_or_fn);
