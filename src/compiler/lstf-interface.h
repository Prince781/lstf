#pragma once

#include "lstf-common.h"
#include "lstf-sourceref.h"
#include "data-structures/ptr-list.h"
#include "lstf-datatype.h"
#include "lstf-symbol.h"
#include "lstf-typesymbol.h"
#include <stdbool.h>
#include <stddef.h>

#if defined(interface)
#undef interface        // MSVC
#endif

struct _lstf_interfaceproperty {
    lstf_symbol parent_struct;

    /**
     * Whether the property can be omitted.
     */
    bool is_optional;
    lstf_datatype *property_type;
};
typedef struct _lstf_interfaceproperty lstf_interfaceproperty;

static inline lstf_interfaceproperty *lstf_interfaceproperty_cast(void *node)
{
    lstf_symbol *symbol = lstf_symbol_cast(node);

    if (symbol && symbol->symbol_type == lstf_symbol_type_interfaceproperty)
        return node;
    return NULL;
}

lstf_interfaceproperty *lstf_interfaceproperty_new(const lstf_sourceref *source_reference,
                                                   const char           *name,
                                                   bool                  is_optional,
                                                   lstf_datatype        *property_type,
                                                   bool                  is_builtin);

void lstf_interfaceproperty_set_property_type(lstf_interfaceproperty *property,
                                              lstf_datatype          *property_type);

struct _lstf_interface {
    lstf_typesymbol parent_struct;

    /**
     * List of interface types that this interface extends
     * 
     * Initially a list of `(lstf_unresolvedtype *)`
     */
    ptr_list *extends_types;

    /**
     * If this is an anonymous interface, then that means it was created for an
     * object expression.
     */
    bool is_anonymous;
};
typedef struct _lstf_interface lstf_interface;

static inline lstf_interface *lstf_interface_cast(void *node)
{
    lstf_typesymbol *type_symbol = lstf_typesymbol_cast(node);

    if (type_symbol && type_symbol->typesymbol_type == lstf_typesymbol_type_interface)
        return node;
    return NULL;
}

lstf_interface *lstf_interface_new(const lstf_sourceref *source_reference,
                                   const char           *name,
                                   bool                  is_anonymous,
                                   bool                  is_builtin);

void lstf_interface_add_base_type(lstf_interface *interface, lstf_datatype *base_type);

void lstf_interface_add_member(lstf_interface *interface, lstf_interfaceproperty *property);

void lstf_interface_replace_base_type(lstf_interface *interface, lstf_datatype *old_base_type, lstf_datatype *new_base_type);
