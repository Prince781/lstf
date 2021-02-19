#include "lstf-function.h"
#include "compiler/lstf-lambdaexpression.h"
#include "compiler/lstf-variable.h"
#include "data-structures/ptr-hashset.h"
#include "lstf-datatype.h"
#include "lstf-codevisitor.h"
#include "lstf-block.h"
#include "lstf-codenode.h"
#include "lstf-scope.h"
#include "lstf-symbol.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include "util.h"
#include <assert.h>
#include <stdio.h>
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
    ptr_hashset_destroy(function->captured_locals);

    lstf_symbol_destruct(node);
}

static const lstf_codenode_vtable function_vtable = {
    lstf_function_accept,
    lstf_function_accept_children,
    lstf_function_destruct
};

static lstf_function *lstf_function_create(const lstf_sourceref *source_reference, 
                                           const char           *name,
                                           lstf_datatype        *return_type,
                                           bool                  is_instance,
                                           bool                  is_async,
                                           bool                  is_builtin)
{
    lstf_function *function = calloc(1, sizeof *function);

    if (!function) {
        perror("failed to create lstf_function");
        abort();
    }

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
    function->captured_locals = ptr_hashset_new(ptrhash,
            (collection_item_ref_func) lstf_codenode_ref, (collection_item_unref_func) lstf_codenode_unref,
            NULL);
    function->is_instance = is_instance;
    function->is_async = is_async;

    if (!is_builtin) {
        function->block = lstf_codenode_ref(lstf_block_new(&(lstf_sourceref) {
                        source_reference ? source_reference->file : NULL,
                        { 0 },
                        { 0 }
                    }));
        lstf_codenode_set_parent(function->block, function);
    }

    return function;
}

lstf_symbol *lstf_function_new(const lstf_sourceref *source_reference, 
                               const char           *name,
                               lstf_datatype        *return_type,
                               bool                  is_instance,
                               bool                  is_async)
{
    return (lstf_symbol *)lstf_function_create(source_reference, name, return_type, is_instance, is_async, false);
}

lstf_symbol *lstf_function_new_for_opcode(const lstf_sourceref *source_reference,
                                          const char           *name,
                                          lstf_datatype        *return_type,
                                          bool                  is_async,
                                          lstf_vm_opcode        vm_opcode,
                                          lstf_vm_vmcallcode    vm_callcode)
{
    lstf_function *function =
        lstf_function_create(source_reference, name, return_type, false, is_async, true);

    function->vm_opcode = vm_opcode;
    function->vm_callcode = vm_callcode;

    return (lstf_symbol *)function;
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
    lstf_block_add_statement(function->block, statement);
}

void lstf_function_add_captured_local(lstf_function *function, lstf_symbol *var_or_fn)
{
    assert(function->block && "cannot capture variable for built-in function");
    assert(lstf_variable_cast(var_or_fn) ||
            (lstf_function_cast(var_or_fn) &&
             !ptr_hashset_is_empty(lstf_function_cast(var_or_fn)->captured_locals)));
    ptr_hashset_insert(function->captured_locals, var_or_fn);
}
