#pragma once

#include "lstf-codenode.h"
#include "lstf-sourceref.h"
#include "lstf-datatype.h"

struct _lstf_unresolvedtype {
    lstf_datatype parent_struct;
    char *name;
};
typedef struct _lstf_unresolvedtype lstf_unresolvedtype;

static inline lstf_unresolvedtype *lstf_unresolvedtype_cast(void *node)
{
    lstf_codenode *code_node = node;

    if (code_node && code_node->codenode_type == lstf_codenode_type_datatype &&
            ((lstf_datatype *)code_node)->datatype_type == lstf_datatype_type_unresolvedtype)
        return (lstf_unresolvedtype *)code_node;
    return NULL;
}

lstf_datatype *lstf_unresolvedtype_new(const lstf_sourceref *source_reference,
                                       const char           *name);
