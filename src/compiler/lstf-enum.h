#pragma once

#include "lstf-common.h"
#include "lstf-datatype.h"
#include "lstf-constant.h"
#include "lstf-sourceref.h"
#include "lstf-typesymbol.h"
#include "lstf-codenode.h"
#include "lstf-symbol.h"
#include <stddef.h>

struct _lstf_enum {
    lstf_typesymbol parent_struct;

    /**
     * The type for all of the members
     */
    lstf_datatype *members_type;
};
/**
 * An `enum` type is a type with 
 */
typedef struct _lstf_enum lstf_enum;

static inline lstf_enum *lstf_enum_cast(void *node)
{
    lstf_codenode *code_node = node;

    if (code_node && ((lstf_codenode *)code_node)->codenode_type == lstf_codenode_type_symbol &&
            ((lstf_symbol *)code_node)->symbol_type == lstf_symbol_type_typesymbol &&
            ((lstf_typesymbol *)code_node)->typesymbol_type == lstf_typesymbol_type_enum)
        return (lstf_enum *)code_node;
    return NULL;
}

lstf_symbol *lstf_enum_new(const lstf_sourceref *source_reference,
                           const char           *name,
                           bool                  is_builtin)
    __attribute__((nonnull (2)));

void lstf_enum_add_member(lstf_enum *self, lstf_constant *member);

void lstf_enum_set_members_type(lstf_enum *self, lstf_datatype *data_type);
