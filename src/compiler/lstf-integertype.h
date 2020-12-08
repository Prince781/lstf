#pragma once

#include "lstf-codenode.h"
#include "lstf-sourceref.h"
#include "lstf-datatype.h"

struct _lstf_integertype {
    lstf_datatype parent_struct;
};
typedef struct _lstf_integertype lstf_integertype;

static inline lstf_integertype *lstf_integertype_cast(void *node)
{
    lstf_datatype *data_type = lstf_datatype_cast(node);

    if (data_type && data_type->datatype_type == lstf_datatype_type_integertype)
        return node;
    return NULL;
}

lstf_datatype *lstf_integertype_new(const lstf_sourceref *source_reference);
