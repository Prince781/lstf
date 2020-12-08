#include "lstf-sourceref.h"
#include "data-structures/ptr-list.h"
#include "lstf-datatype.h"
#include <stddef.h>

struct _lstf_uniontype {
    lstf_datatype parent_struct;

    /**
     * List of data types
     */
    ptr_list *options;
};
typedef struct _lstf_uniontype lstf_uniontype;

static inline lstf_uniontype *lstf_uniontype_cast(void *node)
{
    lstf_datatype *data_type = lstf_datatype_cast(node);

    if (data_type && data_type->datatype_type == lstf_datatype_type_uniontype)
        return node;
    return NULL;
}

lstf_datatype *lstf_uniontype_new(const lstf_sourceref *source_reference,
                                  lstf_datatype        *first_type,
                                  ...)
    __attribute__((nonnull(2), sentinel));

void lstf_uniontype_add_option(lstf_uniontype *union_type, lstf_datatype *type_option);

void lstf_union_type_replace_option(lstf_uniontype *union_type,
                                    lstf_datatype  *old_data_type,
                                    lstf_datatype  *new_data_type);
