#pragma once

#include "lstf-sourceref.h"
#include "lstf-datatype.h"

typedef struct _lstf_arraytype lstf_arraytype;
struct _lstf_arraytype {
    lstf_datatype parent_struct;
    lstf_datatype *element_type;
};

static inline lstf_arraytype *lstf_arraytype_cast(void *node)
{
    lstf_datatype *data_type = lstf_datatype_cast(node);

    if (data_type && data_type->datatype_type == lstf_datatype_type_arraytype)
        return node;
    return NULL;
}

lstf_datatype *lstf_arraytype_new(const lstf_sourceref *source_reference,
                                   lstf_datatype       *element_type)
    __attribute__((nonnull (2)));

void lstf_arraytype_set_element_type(lstf_arraytype *self, lstf_datatype *element_type);
