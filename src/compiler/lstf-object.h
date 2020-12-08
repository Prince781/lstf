#pragma once

#include "data-structures/iterator.h"
#include "lstf-codenode.h"
#include "lstf-sourceref.h"
#include "lstf-expression.h"
#include "lstf-symbol.h"
#include "data-structures/ptr-hashmap.h"
#include <stddef.h>

struct _lstf_objectproperty {
    lstf_symbol parent_struct;
    bool is_nullable;
    lstf_expression *value;
};
typedef struct _lstf_objectproperty lstf_objectproperty;

static inline lstf_objectproperty *lstf_objectproperty_cast(void *node)
{
    lstf_symbol *symbol = lstf_symbol_cast(node);

    if (symbol && symbol->symbol_type == lstf_symbol_type_objectproperty)
        return node;
    return NULL;
}

lstf_objectproperty *lstf_objectproperty_new(const lstf_sourceref *source_reference,
                                             const char           *name,
                                             bool                  is_nullable,
                                             lstf_expression      *value);

struct _lstf_object {
    lstf_expression parent_struct;

    /**
     * list of `(lstf_object_property *)`
     */
    ptr_list *members_list;

    bool is_pattern;
};
typedef struct _lstf_object lstf_object;

lstf_object *lstf_object_new(const lstf_sourceref *source_reference, bool is_pattern);

void lstf_object_add_property(lstf_object *object, lstf_objectproperty *property);

/**
 * Each element is `lstf_objectproperty *`
 */
iterator lstf_object_iterator_create(lstf_object *object);
