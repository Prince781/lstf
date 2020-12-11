#pragma once

#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include "lstf-sourceref.h"
#include "lstf-datatype.h"
#include "lstf-function.h"
#include <stdbool.h>
#include <stddef.h>

struct _lstf_functiontype {
    lstf_datatype parent_struct;
    lstf_datatype *return_type;
    ptr_list *parameter_types;
    ptr_list *parameter_names;
    bool is_async;
};
typedef struct _lstf_functiontype lstf_functiontype;

static inline lstf_functiontype *lstf_functiontype_cast(void *node)
{
    lstf_datatype *data_type = lstf_datatype_cast(node);

    if (data_type && data_type->datatype_type == lstf_datatype_type_functiontype)
        return node;
    return NULL;
}

lstf_datatype *lstf_functiontype_new(const lstf_sourceref *source_reference,
                                     lstf_datatype        *return_type,
                                     bool                  is_async);

lstf_datatype *lstf_functiontype_new_from_function(const lstf_sourceref *source_reference,
                                                   lstf_function        *function);

void lstf_functiontype_add_parameter(lstf_functiontype *function_type,
                                     const char        *parameter_name,
                                     lstf_datatype     *parameter_type);

void lstf_functiontype_set_return_type(lstf_functiontype *function_type,
                                       lstf_datatype     *data_type);

void lstf_functiontype_replace_parameter_type(lstf_functiontype *function_type,
                                              lstf_datatype     *old_data_type,
                                              lstf_datatype     *new_data_type);
