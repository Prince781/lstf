#include "lstf-futuretype.h"
#include "data-structures/ptr-list.h"
#include "data-structures/string-builder.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-datatype.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static void lstf_futuretype_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)code_node);
}

static void lstf_futuretype_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_futuretype *self = (lstf_futuretype *)code_node;

    lstf_codenode_accept(self->wrapped_type, visitor);
}

static void lstf_futuretype_destruct(lstf_codenode *code_node)
{
    lstf_futuretype *self = (lstf_futuretype *)code_node;

    lstf_codenode_unref(self->wrapped_type);
    lstf_datatype_destruct((lstf_datatype *)self);
}

static const lstf_codenode_vtable futuretype_vtable = {
    lstf_futuretype_accept,
    lstf_futuretype_accept_children,
    lstf_futuretype_destruct
};

static bool lstf_futuretype_is_supertype_of(lstf_datatype *self, lstf_datatype *other)
{
    if (other->datatype_type != lstf_datatype_type_future)
        return false;

    return lstf_datatype_is_supertype_of(((lstf_futuretype *)self)->wrapped_type,
            ((lstf_futuretype *)other)->wrapped_type);
}

static lstf_datatype *lstf_futuretype_copy(lstf_datatype *self)
{
    lstf_futuretype *future_type = (lstf_futuretype *)self;

    return lstf_futuretype_new(&lstf_codenode_cast(self)->source_reference, future_type->wrapped_type);
}

static char *lstf_futuretype_to_string(lstf_datatype *self)
{
    (void) self;
    return strdup("future");
}

static bool lstf_futuretype_replace_type_parameter(lstf_datatype *self, lstf_datatype *type_parameter, lstf_datatype *replacement_type)
{
    lstf_futuretype *self_ft = lstf_futuretype_cast(self);
    ptr_list_node *result = ptr_list_replace(self->parameters, type_parameter, NULL, replacement_type);
    assert(result && "attempting to replace a non-existent type parameter");
    self_ft->wrapped_type = lstf_codenode_ref(ptr_list_node_get_data(result, lstf_datatype *));
    return true;
}

static const lstf_datatype_vtable futuretype_datatype_vtable = {
    lstf_futuretype_is_supertype_of,
    lstf_futuretype_copy,
    lstf_futuretype_to_string,
    /* add_type_parameter = */ NULL,
    lstf_futuretype_replace_type_parameter
};

lstf_datatype *lstf_futuretype_new(const lstf_sourceref *source_reference,
                                   lstf_datatype        *wrapped_type)
{
    lstf_futuretype *future_type = calloc(1, sizeof *future_type);

    if (!future_type) {
        perror("failed to allocate LSTF future type");
        abort();
    }

    lstf_datatype_construct((lstf_datatype *)future_type,
            &futuretype_vtable,
            source_reference,
            lstf_datatype_type_future,
            &futuretype_datatype_vtable);

    if (lstf_codenode_cast(wrapped_type)->parent_node)
        wrapped_type = lstf_datatype_copy(wrapped_type);
    future_type->wrapped_type = lstf_codenode_ref(wrapped_type);
    lstf_codenode_set_parent(future_type->wrapped_type, future_type);
    ptr_list_append(lstf_datatype_cast(future_type)->parameters, future_type->wrapped_type);

    return (lstf_datatype *)future_type;
}
