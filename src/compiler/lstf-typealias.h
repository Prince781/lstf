#pragma once

#include "lstf-sourceref.h"
#include "lstf-datatype.h"
#include "lstf-typesymbol.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * A type alias is something like:
 * `type [name] = <data type>;`
 */
struct _lstf_typealias {
    lstf_typesymbol parent_struct;
    lstf_datatype *aliased_type;
};
typedef struct _lstf_typealias lstf_typealias;

static inline lstf_typealias *lstf_typealias_cast(void *node)
{
    lstf_typesymbol *type_symbol = lstf_typesymbol_cast(node);

    if (type_symbol && type_symbol->typesymbol_type == lstf_typesymbol_type_alias)
        return node;
    return NULL;
}

lstf_typesymbol *lstf_typealias_new(const lstf_sourceref *source_reference,
                                    const char           *name,
                                    lstf_datatype        *aliased_type,
                                    bool                  is_builtin);

void lstf_typealias_set_aliased_type(lstf_typealias *self, lstf_datatype *aliased_type);
