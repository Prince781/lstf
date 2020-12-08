#pragma once

#include "lstf-sourceref.h"
#include "lstf-enum.h"
#include "lstf-datatype.h"

struct _lstf_enumtype {
    lstf_datatype parent_struct;
    lstf_enum *enum_symbol;
};
typedef struct _lstf_enumtype lstf_enumtype;

static inline lstf_enumtype *lstf_enumtype_cast(void *node)
{
    lstf_datatype *data_type = lstf_datatype_cast(node);

    if (data_type && data_type->datatype_type == lstf_datatype_type_enumtype)
        return node;
    return NULL;
}

lstf_datatype *lstf_enumtype_new(const lstf_sourceref *source_reference,
                                 lstf_enum            *enum_symbol);
