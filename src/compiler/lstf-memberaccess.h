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

lstf_expression *lstf_memberaccess_new(const lstf_sourceref *source_reference,
                                       lstf_expression *inner,
                                       const char *member_name);
