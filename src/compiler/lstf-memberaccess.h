#pragma once

#include "lstf-sourceref.h"
#include "lstf-expression.h"
#include "lstf-codenode.h"

struct _lstf_memberaccess {
    lstf_expression parent_struct;
    lstf_expression *inner;
    char *member_name;
};
typedef struct _lstf_memberaccess lstf_memberaccess;

static inline lstf_memberaccess *lstf_memberaccess_cast(void *node)
{
    lstf_expression *expr = lstf_expression_cast(node);

    if (expr && expr->expr_type == lstf_expression_type_memberaccess)
        return node;
    return NULL;
}

lstf_expression *lstf_memberaccess_new(const lstf_sourceref *source_reference,
                                       lstf_expression *inner,
                                       const char *member_name);
