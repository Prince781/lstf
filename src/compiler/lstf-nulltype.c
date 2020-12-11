#include "lstf-nulltype.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include "lstf-uniontype.h"
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

static void lstf_nulltype_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)node);
}

static void lstf_nulltype_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    (void) node;
    (void) visitor;
}

static void lstf_nulltype_destruct(lstf_codenode *node)
{
    lstf_datatype_destruct((lstf_datatype *)node);
}

static const lstf_codenode_vtable nulltype_vtable = {
    lstf_nulltype_accept,
    lstf_nulltype_accept_children,
    lstf_nulltype_destruct
};

static bool lstf_nulltype_is_supertype_of(lstf_datatype *self, lstf_datatype *other)
{
    if (other->datatype_type == lstf_datatype_type_nulltype)
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

static lstf_datatype *lstf_nulltype_copy(lstf_datatype *self)
{
    return lstf_nulltype_new(&lstf_codenode_cast(self)->source_reference);
}

static char *lstf_nulltype_to_string(lstf_datatype *self)
{
    (void) self;
    return strdup("null");
}

static const lstf_datatype_vtable nulltype_datatype_vtable = {
    lstf_nulltype_is_supertype_of,
    lstf_nulltype_copy,
    lstf_nulltype_to_string
};

lstf_datatype *lstf_nulltype_new(const lstf_sourceref *source_reference)
{
    lstf_nulltype *null_type = calloc(1, sizeof *null_type);

    lstf_datatype_construct((lstf_datatype *)null_type,
            &nulltype_vtable,
            source_reference,
            lstf_datatype_type_nulltype,
            &nulltype_datatype_vtable);

    return (lstf_datatype *)null_type;
}
