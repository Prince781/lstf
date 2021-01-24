#include "lstf-voidtype.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void lstf_voidtype_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)node);
}

static void lstf_voidtype_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    (void) node;
    (void) visitor;
}

static void lstf_voidtype_destruct(lstf_codenode *node)
{
    lstf_datatype_destruct((lstf_datatype *)node);
}

static const lstf_codenode_vtable voidtype_vtable = {
    lstf_voidtype_accept,
    lstf_voidtype_accept_children,
    lstf_voidtype_destruct
};

static bool lstf_voidtype_is_supertype_of(lstf_datatype *self, lstf_datatype *other)
{
    (void) self;
    return other->datatype_type == lstf_datatype_type_voidtype;
}

static lstf_datatype *lstf_voidtype_copy(lstf_datatype *self)
{
    return lstf_voidtype_new(&lstf_codenode_cast(self)->source_reference);
}

static char *lstf_voidtype_to_string(lstf_datatype *self)
{
    (void) self;
    return strdup("void");
}

static const lstf_datatype_vtable voidtype_datatype_vtable = {
    lstf_voidtype_is_supertype_of,
    lstf_voidtype_copy,
    lstf_voidtype_to_string
};

lstf_datatype *lstf_voidtype_new(const lstf_sourceref *source_reference)
{
    lstf_voidtype *void_type = calloc(1, sizeof *void_type);

    if (!void_type) {
        perror("failed to create lstf_voidtype");
        abort();
    }

    lstf_datatype_construct((lstf_datatype *)void_type,
            &voidtype_vtable,
            source_reference,
            lstf_datatype_type_voidtype,
            &voidtype_datatype_vtable);

    return (lstf_datatype *)void_type;
}
