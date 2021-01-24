#include "lstf-unresolvedtype.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void lstf_unresolvedtype_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)code_node);
}

static void lstf_unresolvedtype_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    (void) code_node;
    (void) visitor;
}

static void lstf_unresolvedtype_destruct(lstf_codenode *code_node)
{
    lstf_unresolvedtype *unresolvedtype = (lstf_unresolvedtype *)code_node;

    free(unresolvedtype->name);
    unresolvedtype->name = NULL;
    lstf_datatype_destruct((lstf_datatype *) code_node);
}

static const lstf_codenode_vtable unresolvedtype_vtable = {
    lstf_unresolvedtype_accept,
    lstf_unresolvedtype_accept_children,
    lstf_unresolvedtype_destruct
};

static bool lstf_unresolvedtype_is_supertype_of(lstf_datatype *self_dt, lstf_datatype *other)
{
    (void) self_dt;
    assert(other != NULL && "cannot compare lstf_unresolvedtype to NULL data type");

    return false;
}

static lstf_datatype *lstf_unresolvedtype_copy(lstf_datatype *self_dt)
{
    lstf_codenode *code_node = (lstf_codenode *)self_dt;
    lstf_unresolvedtype *self = (lstf_unresolvedtype *)self_dt;

    return lstf_unresolvedtype_new(&code_node->source_reference, self->name);
}

static char *lstf_unresolvedtype_to_string(lstf_datatype *self_dt)
{
    lstf_unresolvedtype *self = (lstf_unresolvedtype *)self_dt;

    return strdup(self->name);
}

static const lstf_datatype_vtable unresolvedtype_datatype_vtable = {
    lstf_unresolvedtype_is_supertype_of,
    lstf_unresolvedtype_copy,
    lstf_unresolvedtype_to_string
};

lstf_datatype *lstf_unresolvedtype_new(const lstf_sourceref *source_reference,
                                       const char           *name)
{
    lstf_unresolvedtype *type = calloc(1, sizeof *type);

    if (!type) {
        perror("failed to create lstf_unresolvedtype");
        abort();
    }

    lstf_datatype_construct((lstf_datatype *)type,
            &unresolvedtype_vtable,
            source_reference,
            lstf_datatype_type_unresolvedtype,
            &unresolvedtype_datatype_vtable);

    type->name = strdup(name);
    return (lstf_datatype *)type;
}
