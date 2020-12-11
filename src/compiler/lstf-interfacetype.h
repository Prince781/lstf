#pragma once

#include "lstf-codenode.h"
#include "lstf-sourceref.h"
#include "lstf-interface.h"
#include "lstf-datatype.h"

struct _lstf_interfacetype {
    lstf_datatype parent_struct;
};
typedef struct _lstf_interfacetype lstf_interfacetype;

static inline lstf_interfacetype *lstf_interfacetype_cast(void *node)
{
    lstf_datatype *data_type = lstf_datatype_cast(node);

    if (data_type && data_type->datatype_type == lstf_datatype_type_interfacetype)
        return node;
    return NULL;
}

lstf_datatype *lstf_interfacetype_new(const lstf_sourceref *source_reference,
                                      lstf_interface       *interface);
