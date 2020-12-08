#pragma once

#include "lstf-sourceref.h"
#include "lstf-datatype.h"

struct _lstf_doubletype {
    lstf_datatype parent_struct;
};
typedef struct _lstf_doubletype lstf_doubletype;

static inline lstf_doubletype *lstf_doubletype_cast(void *node)
{
    lstf_datatype *data_type = lstf_datatype_cast(node);

    if (data_type && data_type->datatype_type == lstf_datatype_type_doubletype)
        return node;
    return NULL;
}

lstf_datatype *lstf_doubletype_new(const lstf_sourceref *source_reference);
