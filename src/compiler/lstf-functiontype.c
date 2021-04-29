#include "lstf-functiontype.h"
#include "compiler/lstf-report.h"
#include "compiler/lstf-variable.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include "data-structures/string-builder.h"
#include "lstf-symbol.h"
#include "lstf-datatype.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-uniontype.h"
#include "util.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void lstf_functiontype_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_data_type(visitor, (lstf_datatype *)node);
}

static void lstf_functiontype_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_functiontype *self = (lstf_functiontype *)node;
    for (iterator it = ptr_list_iterator_create(self->parameter_types);
            it.has_next; it = iterator_next(it)) {
        lstf_datatype *parameter_type = iterator_get_item(it);
        lstf_codenode_accept(parameter_type, visitor);
    }
    lstf_codenode_accept(self->return_type, visitor);
}

static void lstf_functiontype_destruct(lstf_codenode *node)
{
    lstf_functiontype *function_type = (lstf_functiontype *)node;

    ptr_list_destroy(function_type->parameter_names);
    function_type->parameter_names = NULL;

    ptr_list_destroy(function_type->parameter_types);
    function_type->parameter_types = NULL;

    lstf_codenode_unref(function_type->return_type);
    function_type->return_type = NULL;

    lstf_datatype_destruct((lstf_datatype *)node);
}

static const lstf_codenode_vtable functiontype_vtable = {
    lstf_functiontype_accept,
    lstf_functiontype_accept_children,
    lstf_functiontype_destruct
};

static bool lstf_functiontype_is_supertype_of(lstf_datatype *self_dt, lstf_datatype *other)
{
    lstf_functiontype *self = (lstf_functiontype *)self_dt;
    lstf_functiontype *other_ft = lstf_functiontype_cast(other);

    if (!other_ft) {
        if (other->datatype_type == lstf_datatype_type_uniontype) {
            for (iterator it = ptr_list_iterator_create(lstf_uniontype_cast(other)->options); it.has_next; it = iterator_next(it)) {
                if (!lstf_datatype_is_supertype_of(self_dt, iterator_get_item(it)))
                    return false;
            }
            return true;
        }
        return false;
    }

    // we can save time if the function types have different numbers of arguments
    if (self->parameter_types->length != other_ft->parameter_types->length)
        return false;

    // now we get to the meat of the algorithm:
    // our rules are:
    //  - covariance on return (output) type
    //  - contravariance on parameter (input) types

    if (!lstf_datatype_is_supertype_of(self->return_type, other_ft->return_type))
        return false;

    iterator it1 = ptr_list_iterator_create(self->parameter_types);
    iterator it2 = ptr_list_iterator_create(other_ft->parameter_types);

    while (it1.has_next && it2.has_next) {
        lstf_datatype *param_type1 = iterator_get_item(it1);
        lstf_datatype *param_type2 = iterator_get_item(it2);

        if (!lstf_datatype_is_supertype_of(param_type2, param_type1))
            return false;

        it1 = iterator_next(it1);
        it2 = iterator_next(it2);
    }

    return true;
}

static lstf_datatype *lstf_functiontype_copy(lstf_datatype *self_dt)
{
    lstf_functiontype *self = (lstf_functiontype *)self_dt;

    lstf_functiontype *new_type = (lstf_functiontype *)
        lstf_functiontype_new(&lstf_codenode_cast(self)->source_reference,
                self->return_type, self->is_async);

    for (iterator it_name = ptr_list_iterator_create(self->parameter_names),
                  it_param = ptr_list_iterator_create(self->parameter_types); 
            it_name.has_next && it_param.has_next;
            it_name = iterator_next(it_name), it_param = iterator_next(it_param)) {
        char *parameter_name = iterator_get_item(it_name);
        lstf_datatype *parameter_type = iterator_get_item(it_param);

        lstf_functiontype_add_parameter(new_type, parameter_name, parameter_type);
    }

    return (lstf_datatype *) new_type;
}

static char *lstf_functiontype_to_string(lstf_datatype *self_dt)
{
    lstf_functiontype *self = (lstf_functiontype *)self_dt;
    string *representation = string_new();

    string_appendf(representation, "%s(", self->is_async ? "async " : "");
    for (iterator it_name = ptr_list_iterator_create(self->parameter_names),
                  it_param = ptr_list_iterator_create(self->parameter_types); 
            it_name.has_next && it_param.has_next;
            it_name = iterator_next(it_name), it_param = iterator_next(it_param)) {
        char *parameter_name = iterator_get_item(it_name);
        lstf_datatype *parameter_type = iterator_get_item(it_param);
        char *param_type_string = lstf_datatype_to_string(parameter_type);

        string_appendf(representation, "%s%s: %s",
                it_name.is_first ? "" : ", ",
                parameter_name,
                param_type_string);

        free(param_type_string);
    }

    char *return_type_string = lstf_datatype_to_string(self->return_type);
    string_appendf(representation, ") => %s", return_type_string);
    free(return_type_string);

    return string_destroy(representation);
}

static const lstf_datatype_vtable functiontype_datatype_vtable = {
    lstf_functiontype_is_supertype_of,
    lstf_functiontype_copy,
    lstf_functiontype_to_string,
    /* add_type_parameter = */ NULL,
    /* replace_type_parameter = */ NULL
};

lstf_datatype *lstf_functiontype_new(const lstf_sourceref *source_reference,
                                     lstf_datatype        *return_type,
                                     bool                  is_async)
{
    lstf_functiontype *function_type = calloc(1, sizeof *function_type);

    if (!function_type) {
        perror("failed to create lstf_functiontype");
        abort();
    }

    lstf_datatype_construct((lstf_datatype *)function_type,
            &functiontype_vtable,
            source_reference,
            lstf_datatype_type_functiontype,
            &functiontype_datatype_vtable);

    if (lstf_codenode_cast(return_type)->parent_node)
        return_type = lstf_datatype_copy(return_type);
    function_type->return_type = lstf_codenode_ref(return_type);
    lstf_codenode_set_parent(function_type->return_type, function_type);

    function_type->parameter_names = ptr_list_new(NULL,
            (collection_item_unref_func) free);
    function_type->parameter_types = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);
    function_type->is_async = is_async;

    return (lstf_datatype *)function_type;
}

lstf_datatype *lstf_functiontype_new_from_function(const lstf_sourceref *source_reference,
                                                   lstf_function        *function)
{
    lstf_functiontype *function_type = (lstf_functiontype *)
        lstf_functiontype_new(source_reference, function->return_type, function->is_async);

    for (iterator it = ptr_list_iterator_create(function->parameters);
            it.has_next; it = iterator_next(it)) {
        lstf_variable *parameter = iterator_get_item(it);

        lstf_functiontype_add_parameter(function_type,
                lstf_symbol_cast(parameter)->name, parameter->variable_type);
    }

    return (lstf_datatype *)function_type;
}

void lstf_functiontype_add_parameter(lstf_functiontype *function_type,
                                     const char        *parameter_name,
                                     lstf_datatype     *parameter_type)
{
    ptr_list_append(function_type->parameter_names, strdup(parameter_name));

    if (lstf_codenode_cast(parameter_type)->parent_node)
        parameter_type = lstf_datatype_copy(parameter_type);
    ptr_list_append(function_type->parameter_types, parameter_type);
    lstf_codenode_set_parent(parameter_type, function_type);
}

void lstf_functiontype_set_return_type(lstf_functiontype *function_type,
                                       lstf_datatype     *data_type)
{
    lstf_codenode_unref(function_type->return_type);

    if (((lstf_codenode *)data_type)->parent_node)
        data_type = lstf_datatype_copy(data_type);
    function_type->return_type = lstf_codenode_ref(data_type);
    lstf_codenode_set_parent(function_type->return_type, function_type);
}

void lstf_functiontype_replace_parameter_type(lstf_functiontype *function_type,
                                              lstf_datatype     *old_data_type,
                                              lstf_datatype     *new_data_type)
{
    ptr_list_node *result =
        ptr_list_replace(function_type->parameter_types, old_data_type, NULL, new_data_type);
    assert(result && "attempting to replace a non-existent parameter type!");
}
