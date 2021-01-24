#include "lstf-typealias.h"
#include "lstf-codevisitor.h"
#include "lstf-datatype.h"
#include "lstf-codenode.h"
#include "lstf-typesymbol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void lstf_typealias_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_type_alias(visitor, (lstf_typealias *)node);
}

static void lstf_typealias_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, ((lstf_typealias *)node)->aliased_type);
}

static void lstf_typealias_destruct(lstf_codenode *node)
{
    lstf_typealias *self = (lstf_typealias *)node;
    lstf_codenode_unref(self->aliased_type);
    self->aliased_type = NULL;
    lstf_typesymbol_destruct(node);
}

static const lstf_codenode_vtable typealias_vtable = {
    lstf_typealias_accept,
    lstf_typealias_accept_children,
    lstf_typealias_destruct
};

lstf_typesymbol *lstf_typealias_new(const lstf_sourceref *source_reference,
                                    const char           *name,
                                    lstf_datatype        *aliased_type,
                                    bool                  is_builtin)
{
    lstf_typealias *type_alias = calloc(1, sizeof *type_alias);

    if (!type_alias) {
        perror("failed to create lstf_typealias");
        abort();
    }

    lstf_typesymbol_construct((lstf_typesymbol *)type_alias,
            &typealias_vtable,
            source_reference,
            lstf_typesymbol_type_alias,
            strdup(name),
            is_builtin);

    lstf_typealias_set_aliased_type(type_alias, aliased_type);
    
    return (lstf_typesymbol *) type_alias;
}

void lstf_typealias_set_aliased_type(lstf_typealias *self, lstf_datatype *aliased_type)
{
    lstf_codenode_unref(self->aliased_type);
    if (lstf_codenode_cast(aliased_type)->parent_node)
        aliased_type = lstf_datatype_copy(aliased_type);
    self->aliased_type = lstf_codenode_ref(aliased_type);
    lstf_codenode_set_parent(self->aliased_type, self);
    lstf_datatype_set_symbol(self->aliased_type, self);
}
