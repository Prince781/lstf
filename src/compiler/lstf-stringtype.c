#include "lstf-stringtype.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include "lstf-sourceref.h"
#include <string.h>
#include <stdlib.h>

static void lstf_stringtype_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)node);
}

static void lstf_stringtype_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    (void) node;
    (void) visitor;
}

static void lstf_stringtype_destruct(lstf_codenode *node)
{
    lstf_datatype_destruct((lstf_datatype *)node);
}

static const lstf_codenode_vtable stringtype_vtable = {
    lstf_stringtype_accept,
    lstf_stringtype_accept_children,
    lstf_stringtype_destruct
};

static bool lstf_stringtype_is_supertype_of(lstf_datatype *self, lstf_datatype *other)
{
    (void) self;
    return !!lstf_stringtype_cast(other);
}

static lstf_datatype *lstf_stringtype_copy(lstf_datatype *self)
{
    return lstf_stringtype_new(&((lstf_codenode *)self)->source_reference);
}

static char *lstf_stringtype_to_string(lstf_datatype *self)
{
    (void) self;
    return strdup("string");
}

static const lstf_datatype_vtable stringtype_datatype_vtable = {
    lstf_stringtype_is_supertype_of,
    lstf_stringtype_copy,
    lstf_stringtype_to_string
};

lstf_datatype *lstf_stringtype_new(const lstf_sourceref *source_reference)
{
    lstf_stringtype *string_type = calloc(1, sizeof *string_type);

    lstf_datatype_construct((lstf_datatype *)string_type,
            &stringtype_vtable,
            source_reference,
            lstf_datatype_type_stringtype,
            &stringtype_datatype_vtable);

    return (lstf_datatype *)string_type;
}
