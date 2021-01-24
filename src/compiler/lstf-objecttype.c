#include "lstf-objecttype.h"
#include "lstf-codevisitor.h"
#include "lstf-interfacetype.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include "lstf-uniontype.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void lstf_objecttype_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)node);
}

static void lstf_objecttype_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    (void) node;
    (void) visitor;
}

static void lstf_objecttype_destruct(lstf_codenode *node)
{
    lstf_datatype_destruct((lstf_datatype *)node);
}

static const lstf_codenode_vtable objecttype_vtable = {
    lstf_objecttype_accept,
    lstf_objecttype_accept_children,
    lstf_objecttype_destruct
};

static bool lstf_objecttype_is_supertype_of(lstf_datatype *self, lstf_datatype *other)
{
    if (other->datatype_type == lstf_datatype_type_objecttype ||
            other->datatype_type == lstf_datatype_type_interfacetype)
        return true;

    if (other->datatype_type == lstf_datatype_type_uniontype) {
        for (iterator it = ptr_list_iterator_create(lstf_uniontype_cast(other)->options); it.has_next; it = iterator_next(it)) {
            if (!lstf_datatype_is_supertype_of(self, iterator_get_item(it)))
                return false;
        }
        return true;
    }

    return false;
}

static lstf_datatype *lstf_objecttype_copy(lstf_datatype *self)
{
    return lstf_objecttype_new(&((lstf_codenode *)self)->source_reference);
}

static char *lstf_objecttype_to_string(lstf_datatype *self)
{
    (void) self;
    return strdup("object");
}

static const lstf_datatype_vtable objecttype_datatype_vtable = {
    lstf_objecttype_is_supertype_of,
    lstf_objecttype_copy,
    lstf_objecttype_to_string
};

lstf_datatype *lstf_objecttype_new(const lstf_sourceref *source_reference)
{
    lstf_objecttype *object_type = calloc(1, sizeof *object_type);

    if (!object_type) {
        perror("failed to create lstf_objecttype");
        abort();
    }

    lstf_datatype_construct((lstf_datatype *)object_type,
            &objecttype_vtable,
            source_reference,
            lstf_datatype_type_objecttype,
            &objecttype_datatype_vtable);

    return (lstf_datatype *)object_type;
}
