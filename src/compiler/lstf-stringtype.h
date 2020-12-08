#pragma once

#include "lstf-sourceref.h"
#include "lstf-datatype.h"

struct _lstf_stringtype {
    lstf_datatype parent_struct;
};
typedef struct _lstf_stringtype lstf_stringtype;

static inline lstf_stringtype *lstf_stringtype_cast(void *node)
{
    lstf_datatype *data_type = lstf_datatype_cast(node);

    if (data_type && data_type->datatype_type == lstf_datatype_type_stringtype)
        return node;
    return NULL;
}

lstf_datatype *lstf_stringtype_new(const lstf_sourceref *source_reference);
