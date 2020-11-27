#pragma once

#include "lstf-datatype.h"
#include "lstf-symbol.h"
#include "lstf-codenode.h"
#include "lstf-sourceref.h"
#include <stddef.h>

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
 * Abstract struct
 */
struct _lstf_expression {
    lstf_codenode parent_struct;

    /**
     * The subclass type
     */
    lstf_expression_type expr_type;

    /**
     * (weak ref) The symbol this expression refers to
     */
    lstf_symbol *symbol_reference;

    /**
     * Statically-determined value type (created by semantic analyzer)
     */
    lstf_datatype *value_type;
};
typedef struct _lstf_expression lstf_expression;

static inline lstf_expression *lstf_expression_cast(void *node)
{
    lstf_codenode *code_node = node;

    if (code_node && code_node->codenode_type == lstf_codenode_type_expression)
        return (lstf_expression *)code_node;
    return NULL;
}

void lstf_expression_construct(lstf_expression            *expr,
                               const lstf_codenode_vtable *vtable,
                               const lstf_sourceref       *source_reference,
                               lstf_expression_type        expr_type);

void lstf_expression_destruct(lstf_codenode *node);
