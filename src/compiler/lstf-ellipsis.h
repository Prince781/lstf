#pragma once

#include "lstf-codenode.h"
#include "lstf-sourceref.h"
#include "lstf-expression.h"

struct _lstf_ellipsis {
    lstf_expression parent_struct;
};
typedef struct _lstf_ellipsis lstf_ellipsis;

lstf_expression *lstf_ellipsis_new(const lstf_sourceref *source_reference);
