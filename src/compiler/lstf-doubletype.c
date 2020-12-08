#include "lstf-doubletype.h"
#include "lstf-codevisitor.h"
#include "lstf-integertype.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include <string.h>
#include <stdlib.h>

static void lstf_doubletype_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)node);
}

static void lstf_doubletype_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    (void) node;
    (void) visitor;
}

static void lstf_doubletype_destruct(lstf_codenode *node)
{
    lstf_datatype_destruct((lstf_datatype *)node);
}

static const lstf_codenode_vtable doubletype_vtable = {
    lstf_doubletype_accept,
    lstf_doubletype_accept_children,
    lstf_doubletype_destruct
};

static bool lstf_doubletype_is_supertype_of(lstf_datatype *self, lstf_datatype *other)
{
    (void) self;
    return !!(lstf_doubletype_cast(other) || lstf_integertype_cast(other));
}

static lstf_datatype *lstf_doubletype_copy(lstf_datatype *self)
{
    return lstf_doubletype_new(&((lstf_codenode *)self)->source_reference);
}

static char *lstf_doubletype_to_string(lstf_datatype *self)
{
    (void) self;
    return strdup("double");
}

static const lstf_datatype_vtable doubletype_datatype_vtable = {
    lstf_doubletype_is_supertype_of,
    lstf_doubletype_copy,
    lstf_doubletype_to_string
};

lstf_datatype *lstf_doubletype_new(const lstf_sourceref *source_reference)
{
    lstf_doubletype *double_type = calloc(1, sizeof *double_type);

    lstf_datatype_construct((lstf_datatype *)double_type,
            &doubletype_vtable,
            source_reference,
            lstf_datatype_type_doubletype,
            &doubletype_datatype_vtable);

    return (lstf_datatype *)double_type;
}
