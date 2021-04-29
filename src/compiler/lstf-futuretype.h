#pragma once

#include "lstf-sourceref.h"
#include "lstf-datatype.h"
#include <stddef.h>

/**
 * future<T>
 */
struct _lstf_futuretype {
    lstf_datatype parent_struct;

    /**
     * the `T` for `future<T>`
     */
    lstf_datatype *wrapped_type;
};
typedef struct _lstf_futuretype lstf_futuretype;

static inline lstf_futuretype *lstf_futuretype_cast(void *node)
{
    lstf_datatype *data_type = lstf_datatype_cast(node);

    if (data_type && data_type->datatype_type == lstf_datatype_type_future)
        return node;
    return NULL;
}

lstf_datatype *lstf_futuretype_new(const lstf_sourceref *source_reference,
                                   lstf_datatype        *wrapped_type);
