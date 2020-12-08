#include "lstf-numbertype.h"
#include "lstf-booleantype.h"
#include "lstf-doubletype.h"
#include "lstf-integertype.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
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
    (void) self;
    return !!(lstf_numbertype_cast(other) || lstf_integertype_cast(other) ||
            lstf_doubletype_cast(other) || lstf_booleantype_cast(other));
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

    lstf_datatype_construct((lstf_datatype *)number_type,
            &numbertype_vtable,
            source_reference,
            lstf_datatype_type_numbertype,
            &numbertype_datatype_vtable);

    return (lstf_datatype *)number_type;
}
