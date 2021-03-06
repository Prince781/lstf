#pragma once

#include "lstf-sourceref.h"
#include "lstf-datatype.h"

struct _lstf_booleantype {
    lstf_datatype parent_struct;
};
typedef struct _lstf_booleantype lstf_booleantype;

static inline lstf_booleantype *lstf_booleantype_cast(void *node)
{
    lstf_datatype *data_type = lstf_datatype_cast(node);

    if (data_type && data_type->datatype_type == lstf_datatype_type_booleantype)
        return node;
    return NULL;
}

lstf_datatype *lstf_booleantype_new(const lstf_sourceref *source_reference);
