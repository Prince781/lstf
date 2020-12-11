#include "lstf-booleantype.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include "lstf-uniontype.h"
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

static void lstf_booleantype_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)node);
}

static void lstf_booleantype_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    (void) node;
    (void) visitor;
}

static void lstf_booleantype_destruct(lstf_codenode *node)
{
    lstf_datatype_destruct((lstf_datatype *)node);
}

static const lstf_codenode_vtable booleantype_vtable = {
    lstf_booleantype_accept,
    lstf_booleantype_accept_children,
    lstf_booleantype_destruct
};

static bool lstf_booleantype_is_supertype_of(lstf_datatype *self, lstf_datatype *other)
{
    if (other->datatype_type == lstf_datatype_type_booleantype)
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

static lstf_datatype *lstf_booleantype_copy(lstf_datatype *self)
{
    return lstf_booleantype_new(&((lstf_codenode *)self)->source_reference);
}

static char *lstf_booleantype_to_string(lstf_datatype *self)
{
    (void) self;
    return strdup("boolean");
}

static const lstf_datatype_vtable booleantype_datatype_vtable = {
    lstf_booleantype_is_supertype_of,
    lstf_booleantype_copy,
    lstf_booleantype_to_string
};

lstf_datatype *lstf_booleantype_new(const lstf_sourceref *source_reference)
{
    lstf_booleantype *bool_type = calloc(1, sizeof *bool_type);

    lstf_datatype_construct((lstf_datatype *)bool_type,
            &booleantype_vtable,
            source_reference,
            lstf_datatype_type_booleantype,
            &booleantype_datatype_vtable);

    return (lstf_datatype *)bool_type;
}
