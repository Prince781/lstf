#include "lstf-anytype.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void lstf_anytype_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)code_node);
}

static void lstf_anytype_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    (void) code_node;
    (void) visitor;
}

static void lstf_anytype_destruct(lstf_codenode *code_node)
{
    lstf_datatype_destruct((lstf_datatype *)code_node);
}

static const lstf_codenode_vtable anytype_vtable = {
    lstf_anytype_accept,
    lstf_anytype_accept_children,
    lstf_anytype_destruct
};

static bool lstf_anytype_is_supertype_of(lstf_datatype *self, lstf_datatype *other)
{
    (void) self;
    assert(other != NULL && "cannot compare lstf_anytype to NULL data type");
    // any must be a value, so it's incompatible with void and future<T> (for now at least)

    return !(other->datatype_type == lstf_datatype_type_voidtype || other->datatype_type == lstf_datatype_type_future);
}

static lstf_datatype *lstf_anytype_copy(lstf_datatype *self)
{
    return lstf_anytype_new(&((lstf_codenode *)self)->source_reference);
}

static char *lstf_anytype_to_string(lstf_datatype *self)
{
    (void) self;
    return strdup("any");
}

static const lstf_datatype_vtable anytype_datatype_vtable = {
    lstf_anytype_is_supertype_of,
    lstf_anytype_copy,
    lstf_anytype_to_string,
    /* add_type_parameter = */ NULL,
    /* replace_type_parameter = */ NULL
};

lstf_datatype *lstf_anytype_new(const lstf_sourceref *source_reference)
{
    lstf_anytype *anytype = calloc(1, sizeof *anytype);

    if (!anytype) {
        perror("failed to create lstf_anytype");
        abort();
    }

    lstf_datatype_construct((lstf_datatype *)anytype,
            &anytype_vtable,
            source_reference,
            lstf_datatype_type_anytype,
            &anytype_datatype_vtable);

    return (lstf_datatype *)anytype;
}
