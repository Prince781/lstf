#pragma once

#include "lstf-codenode.h"
#include "lstf-sourceref.h"
#include "lstf-datatype.h"

enum _lstf_expression_type {
    lstf_expression_type_array,
    lstf_expression_type_ellipsis,
    lstf_expression_type_literal,
    lstf_expression_type_memberaccess,
    lstf_expression_type_elementaccess,
    lstf_expression_type_methodcall,
    lstf_expression_type_object
};
typedef enum _lstf_expression_type lstf_expression_type;

const char *lstf_expression_type_to_string(lstf_expression_type expr_type);

/**
 * Abstract class
 */
struct _lstf_expression {
    lstf_codenode parent_struct;

    /**
     * The subclass type
     */
    lstf_expression_type expr_type;

    /**
     * The type of the result of evaluating this expression.
     */
    // lstf_datatype value_type;
};
typedef struct _lstf_expression lstf_expression;

void lstf_expression_construct(lstf_expression          *expr,
                               const lstf_sourceref     *source_reference,
                               lstf_codenode_dtor_func   dtor_func,
                               lstf_expression_type      expr_type);
