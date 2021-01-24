#pragma once

#include "lstf-common.h"
#include "lstf-datatype.h"
#include "lstf-symbol.h"
#include "lstf-codenode.h"
#include "lstf-sourceref.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

enum _lstf_expression_type {
    lstf_expression_type_array,
    lstf_expression_type_ellipsis,
    lstf_expression_type_literal,
    lstf_expression_type_memberaccess,
    lstf_expression_type_elementaccess,
    lstf_expression_type_methodcall,
    lstf_expression_type_object,
    lstf_expression_type_unary,
    lstf_expression_type_binary,
    lstf_expression_type_conditional,
    lstf_expression_type_lambda
};
typedef enum _lstf_expression_type lstf_expression_type;

static inline const char *
lstf_expression_type_to_string(lstf_expression_type expr_type)
{
    switch (expr_type) {
        case lstf_expression_type_array:
            return "array expression";
        case lstf_expression_type_elementaccess:
            return "element access expression";
        case lstf_expression_type_ellipsis:
            return "ellipsis expression";
        case lstf_expression_type_literal:
            return "literal expression";
        case lstf_expression_type_memberaccess:
            return "member access expression";
        case lstf_expression_type_methodcall:
            return "method call";
        case lstf_expression_type_object:
            return "object expression";
        case lstf_expression_type_unary:
            return "unary expression";
        case lstf_expression_type_binary:
            return "binary expression";
        case lstf_expression_type_conditional:
            return "conditional expression";
        case lstf_expression_type_lambda:
            return "lambda expression";
    }

    fprintf(stderr, "%s: invalid value `%u' for lstf_expression_type", __func__, expr_type);
    abort();
}

/**
 * Abstract struct
 */
struct _lstf_expression {
    lstf_codenode parent_struct;

    /**
     * The subclass type
     */
    lstf_expression_type expr_type;

    bool is_constant;

    /**
     * (weak ref) The symbol this expression refers to
     */
    lstf_symbol *symbol_reference;

    /**
     * Computed by the semantic analyzer.
     */
    lstf_datatype *value_type;
};
typedef struct _lstf_expression lstf_expression;

static inline lstf_expression *lstf_expression_cast(void *node)
{
    lstf_codenode *code_node = node;

    if (code_node && code_node->codenode_type == lstf_codenode_type_expression) {
        lstf_expression *expression = node;
        switch (expression->expr_type) {
            case lstf_expression_type_array:
            case lstf_expression_type_binary:
            case lstf_expression_type_conditional:
            case lstf_expression_type_elementaccess:
            case lstf_expression_type_ellipsis:
            case lstf_expression_type_lambda:
            case lstf_expression_type_literal:
            case lstf_expression_type_memberaccess:
            case lstf_expression_type_methodcall:
            case lstf_expression_type_object:
            case lstf_expression_type_unary:
                return node;
        }
    }

    return NULL;
}

void lstf_expression_construct(lstf_expression            *expr,
                               const lstf_codenode_vtable *vtable,
                               const lstf_sourceref       *source_reference,
                               lstf_expression_type        expr_type)
    __attribute__((nonnull (1, 2)));

void lstf_expression_destruct(lstf_codenode *node);

void lstf_expression_set_value_type(lstf_expression *expression, lstf_datatype *data_type)
    __attribute__((nonnull (1, 2)));
