#include "lstf-function.h"
#include "lstf-datatype.h"
#include "lstf-codevisitor.h"
#include "lstf-block.h"
#include "lstf-codenode.h"
#include "lstf-scope.h"
#include "lstf-symbol.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static void lstf_function_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_function *function = (lstf_function *) node;

    lstf_codevisitor_visit_function(visitor, function);
}

static void lstf_function_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_function *function = (lstf_function *)node;

    lstf_codenode_accept(function->return_type, visitor);

    for (iterator it = ptr_list_iterator_create(function->parameters); it.has_next; it = iterator_next(it))
        lstf_codenode_accept(iterator_get_item(it), visitor);

    if (function->block) {
        lstf_codenode_accept(function->block, visitor);
    } else {
        assert(((lstf_symbol *)function)->is_builtin && "non-builtin function must have block");
    }
}

static void lstf_function_destruct(lstf_codenode *node)
{
    lstf_function *function = (lstf_function *)node;

    ptr_list_destroy(function->parameters);
    lstf_codenode_unref(function->return_type);
    lstf_codenode_unref(function->block);
    lstf_codenode_unref(function->scope);

    lstf_symbol_destruct(node);
}

static const lstf_codenode_vtable function_vtable = {
    lstf_function_accept,
    lstf_function_accept_children,
    lstf_function_destruct
};

lstf_symbol *lstf_function_new(const lstf_sourceref *source_reference, 
                               const char           *name,
                               lstf_datatype        *return_type,
                               bool                  is_instance,
                               bool                  is_builtin, 
                               bool                  is_async)
{
    lstf_function *function = calloc(1, sizeof *function);

    lstf_symbol_construct(((lstf_symbol *)function),
            &function_vtable,
            source_reference,
            lstf_symbol_type_function,
            strdup(name),
            is_builtin);

    function->parameters = ptr_list_new((collection_item_ref_func) lstf_codenode_ref, 
            (collection_item_unref_func) lstf_codenode_unref);
    function->scope = lstf_codenode_ref(lstf_scope_new((lstf_codenode *)function));
    lstf_function_set_return_type(function, return_type);
    function->is_instance = is_instance;
    function->is_async = is_async;

    if (!is_builtin) {
        function->block = lstf_codenode_ref(lstf_block_new());
        lstf_codenode_set_parent(function->block, function);
    }

    return lstf_symbol_cast(function);
}

void lstf_function_add_parameter(lstf_function *function, lstf_variable *variable)
{
    ptr_list_append(function->parameters, variable);
    lstf_codenode_set_parent(variable, function);
}

static bool compare_parameter_name_to_parameter(const void *param, void *var)
{
    const char *parameter_name = param;

    return strcmp(parameter_name, lstf_symbol_cast(var)->name);
}

lstf_variable *lstf_function_get_parameter(lstf_function *function, const char *parameter_name)
{
    ptr_list_node *found = ptr_list_find(function->parameters, parameter_name,
            (collection_item_equality_func) compare_parameter_name_to_parameter);

    return found ? ptr_list_node_get_data(found, lstf_variable *) : NULL;
}

void lstf_function_set_return_type(lstf_function *function, lstf_datatype *data_type)
{
    lstf_codenode_unref(function->return_type);

    if (((lstf_codenode *)data_type)->parent_node) {
        function->return_type = lstf_codenode_ref(lstf_datatype_copy(data_type));
    } else {
        function->return_type = lstf_codenode_ref(data_type);
    }

    lstf_codenode_set_parent(function->return_type, function);
}

void lstf_function_add_statement(lstf_function *function, lstf_statement *statement)
{
    assert(function->block && "cannot add statement to built-in function");
    ptr_list_append(function->block->statement_list, statement);
    lstf_codenode_set_parent(statement, function->block);
}
