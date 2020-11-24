#include "lstf-expression.h"
#include "lstf-codenode.h"
#include <stdlib.h>
#include <stdio.h>

const char *lstf_expression_type_to_string(lstf_expression_type expr_type)
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
    }

    fprintf(stderr, "%s: invalid value (%d) for lstf_expression_type", __func__, expr_type);
    abort();
}

void lstf_expression_construct(lstf_expression          *expr,
                               const lstf_sourceref     *source_reference,
                               lstf_codenode_dtor_func   dtor_func,
                               lstf_expression_type      expr_type)
{
    lstf_codenode_construct((lstf_codenode *)expr,
            lstf_codenode_type_expression, 
            source_reference,
            dtor_func);
    expr->expr_type = expr_type;
}
