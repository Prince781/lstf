#include "lstf-integertype.h"
#include "lstf-booleantype.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include "lstf-uniontype.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void lstf_integertype_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)node);
}

static void lstf_integertype_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    (void) node;
    (void) visitor;
}

static void lstf_integertype_destruct(lstf_codenode *node)
{
    lstf_datatype_destruct((lstf_datatype *)node);
}

static const lstf_codenode_vtable integertype_vtable = {
    lstf_integertype_accept,
    lstf_integertype_accept_children,
    lstf_integertype_destruct
};

static bool lstf_integertype_is_supertype_of(lstf_datatype *self, lstf_datatype *other)
{
    if (other->datatype_type == lstf_datatype_type_integertype ||
            other->datatype_type == lstf_datatype_type_booleantype)
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

static lstf_datatype *lstf_integertype_copy(lstf_datatype *self)
{
    return lstf_integertype_new(&((lstf_codenode *)self)->source_reference);
}

static char *lstf_integertype_to_string(lstf_datatype *self)
{
    (void) self;
    return strdup("int");
}

static const lstf_datatype_vtable integertype_datatype_vtable = {
    lstf_integertype_is_supertype_of,
    lstf_integertype_copy,
    lstf_integertype_to_string
};

lstf_datatype *lstf_integertype_new(const lstf_sourceref *source_reference)
{
    lstf_integertype *integer_type = calloc(1, sizeof *integer_type);

    if (!integer_type) {
        perror("failed to create lstf_integertype");
        abort();
    }

    lstf_datatype_construct((lstf_datatype *)integer_type,
            &integertype_vtable,
            source_reference,
            lstf_datatype_type_integertype,
            &integertype_datatype_vtable);

    return (lstf_datatype *)integer_type;
}
