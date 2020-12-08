#include "lstf-arraytype.h"
#include "data-structures/string-builder.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include <stdbool.h>
#include <stdlib.h>

static void lstf_arraytype_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)node);
}

static void lstf_arraytype_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, ((lstf_arraytype *)node)->element_type);
}

static void lstf_arraytype_destruct(lstf_codenode *node)
{
    lstf_arraytype *array_type = (lstf_arraytype *)node;

    lstf_codenode_unref(array_type->element_type);
    array_type->element_type = NULL;
    lstf_datatype_destruct((lstf_datatype *)array_type);
}

static const lstf_codenode_vtable arraytype_vtable = {
    lstf_arraytype_accept,
    lstf_arraytype_accept_children,
    lstf_arraytype_destruct
};

static bool lstf_arraytype_is_supertype_of(lstf_datatype *self, lstf_datatype *other)
{
    // array types are invariant, because arrays have producer (ex: T get()) and
    // consumer (ex: push(T)) members
    lstf_arraytype *other_arraytype = lstf_arraytype_cast(other);

    if (!other_arraytype)
        return false;

    return lstf_datatype_equals(((lstf_arraytype *)self)->element_type, other_arraytype->element_type);
}

static lstf_datatype *lstf_arraytype_copy(lstf_datatype *self_dt)
{
    lstf_arraytype *self = (lstf_arraytype *)self_dt;

    return lstf_arraytype_new(&((lstf_codenode *)self)->source_reference,
            lstf_datatype_copy(self->element_type));
}

static char *lstf_arraytype_to_string(lstf_datatype *self_dt)
{
    lstf_arraytype *self = (lstf_arraytype *)self_dt;
    string *representation = string_new();
    char *element_type_to_string = lstf_datatype_to_string(self->element_type);

    if (self->element_type->datatype_type == lstf_datatype_type_uniontype)
        string_appendf(representation, "(%s)[]", element_type_to_string);
    else
        string_appendf(representation, "%s[]", element_type_to_string);
    free(element_type_to_string);

    return string_destroy(representation);
}

static const lstf_datatype_vtable arraytype_datatype_vtable = {
    lstf_arraytype_is_supertype_of,
    lstf_arraytype_copy,
    lstf_arraytype_to_string
};

lstf_datatype *lstf_arraytype_new(const lstf_sourceref *source_reference,
                                   lstf_datatype       *element_type)
{
    lstf_arraytype *array_type = calloc(1, sizeof *array_type);

    lstf_datatype_construct((lstf_datatype *)array_type,
            &arraytype_vtable,
            source_reference,
            lstf_datatype_type_arraytype,
            &arraytype_datatype_vtable);

    array_type->element_type = lstf_codenode_ref(element_type);
    return (lstf_datatype *)array_type;
}
