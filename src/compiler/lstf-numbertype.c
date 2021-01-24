#include "lstf-numbertype.h"
#include "lstf-booleantype.h"
#include "lstf-doubletype.h"
#include "lstf-integertype.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include "lstf-uniontype.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void lstf_numbertype_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)node);
}

static void lstf_numbertype_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    (void) node;
    (void) visitor;
}

static void lstf_numbertype_destruct(lstf_codenode *node)
{
    lstf_datatype_destruct((lstf_datatype *)node);
}

static const lstf_codenode_vtable numbertype_vtable = {
    lstf_numbertype_accept,
    lstf_numbertype_accept_children,
    lstf_numbertype_destruct
};

static bool lstf_numbertype_is_supertype_of(lstf_datatype *self, lstf_datatype *other)
{
    if (other->datatype_type == lstf_datatype_type_numbertype ||
            other->datatype_type == lstf_datatype_type_integertype ||
            other->datatype_type == lstf_datatype_type_doubletype ||
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

static lstf_datatype *lstf_numbertype_copy(lstf_datatype *self)
{
    return lstf_numbertype_new(&((lstf_codenode *)self)->source_reference);
}

static char *lstf_numbertype_to_string(lstf_datatype *self)
{
    (void) self;
    return strdup("number");
}

static const lstf_datatype_vtable numbertype_datatype_vtable = {
    lstf_numbertype_is_supertype_of,
    lstf_numbertype_copy,
    lstf_numbertype_to_string
};

lstf_datatype *lstf_numbertype_new(const lstf_sourceref *source_reference)
{
    lstf_numbertype *number_type = calloc(1, sizeof *number_type);

    if (!number_type) {
        perror("failed to create lstf_numbertype");
        abort();
    }

    lstf_datatype_construct((lstf_datatype *)number_type,
            &numbertype_vtable,
            source_reference,
            lstf_datatype_type_numbertype,
            &numbertype_datatype_vtable);

    return (lstf_datatype *)number_type;
}
