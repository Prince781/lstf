#pragma once

#include "lstf-sourceref.h"
#include "lstf-datatype.h"

/**
 * A type for a generic object. Is a supertype of any interface type.
 */
struct _lstf_objecttype {
    lstf_datatype parent_struct;
};
typedef struct _lstf_objecttype lstf_objecttype;

static inline lstf_objecttype *lstf_objecttype_cast(void *node)
{
    lstf_datatype *data_type = lstf_datatype_cast(node);

    if (data_type && data_type->datatype_type == lstf_datatype_type_objecttype)
        return node;
    return NULL;
}

lstf_datatype *lstf_objecttype_new(const lstf_sourceref *source_reference);
