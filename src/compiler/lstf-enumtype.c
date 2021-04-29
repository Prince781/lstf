#include "lstf-enumtype.h"
#include "data-structures/string-builder.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void lstf_enumtype_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)node);
}

static void lstf_enumtype_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    (void) node;
    (void) visitor;
}

static void lstf_enumtype_destruct(lstf_codenode *node)
{
    lstf_enumtype *enum_type = (lstf_enumtype *)node;

    lstf_codenode_unref(enum_type->enum_symbol);
    enum_type->enum_symbol = NULL;
    lstf_datatype_destruct((lstf_datatype *)enum_type);
}

static const lstf_codenode_vtable enumtype_vtable = {
    lstf_enumtype_accept,
    lstf_enumtype_accept_children,
    lstf_enumtype_destruct
};

static bool lstf_enumtype_is_supertype_of(lstf_datatype *self_dt, lstf_datatype *other)
{
    lstf_enumtype *self = (lstf_enumtype *)self_dt;
    lstf_enumtype *other_et = lstf_enumtype_cast(other);

    if (!other_et)
        return false;

    return other_et->enum_symbol == self->enum_symbol;
}

static lstf_datatype *lstf_enumtype_copy(lstf_datatype *self_dt)
{
    lstf_enumtype *self = (lstf_enumtype *)self_dt;

    return lstf_enumtype_new(&((lstf_codenode *)self)->source_reference, self->enum_symbol);
}

static char *lstf_enumtype_to_string(lstf_datatype *self_dt)
{
    return strdup(lstf_symbol_cast(((lstf_enumtype *)self_dt)->enum_symbol)->name);
}

static const lstf_datatype_vtable enumtype_datatype_vtable = {
    lstf_enumtype_is_supertype_of,
    lstf_enumtype_copy,
    lstf_enumtype_to_string,
    /* add_type_parameter = */ NULL,
    /* replace_type_parameter = */ NULL
};

lstf_datatype *lstf_enumtype_new(const lstf_sourceref *source_reference,
                                 lstf_enum            *enum_symbol)
{
    lstf_enumtype *enum_type = calloc(1, sizeof *enum_type);

    if (!enum_type) {
        perror("failed to create lstf_enumtype");
        abort();
    }

    lstf_datatype_construct((lstf_datatype *)enum_type,
            &enumtype_vtable,
            source_reference,
            lstf_datatype_type_enumtype,
            &enumtype_datatype_vtable);

    enum_type->enum_symbol = lstf_codenode_ref(enum_symbol);
    return (lstf_datatype *)enum_type;
}
