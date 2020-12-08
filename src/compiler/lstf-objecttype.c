#include "lstf-objecttype.h"
#include "lstf-codevisitor.h"
#include "lstf-interfacetype.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
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
    (void) self;
    return !!(lstf_objecttype_cast(other) || lstf_interfacetype_cast(other));
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

    lstf_datatype_construct((lstf_datatype *)object_type,
            &objecttype_vtable,
            source_reference,
            lstf_datatype_type_objecttype,
            &objecttype_datatype_vtable);

    return (lstf_datatype *)object_type;
}
