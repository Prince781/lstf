#include "lstf-function.h"
#include "compiler/lstf-codevisitor.h"
#include "data-structures/iterator.h"
#include "lstf-block.h"
#include "lstf-codenode.h"
#include "lstf-scope.h"
#include "data-structures/ptr-list.h"
#include "lstf-symbol.h"
#include <assert.h>
#include <stdlib.h>

static void lstf_function_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_function *function = lstf_function_cast(node);

    lstf_codevisitor_visit_function(visitor, function);
}

static void lstf_function_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_function *function = lstf_function_cast(node);

    for (iterator it = ptr_list_iterator_create(function->parameters); it.has_next; it = iterator_next(it))
        lstf_codevisitor_visit_variable(visitor, lstf_variable_cast(iterator_get_item(it)));

    if (function->block) {
        lstf_codevisitor_visit_block(visitor, function->block);
    } else {
        assert(function->is_builtin && "non-builtin function must have block");
    }
}

static void lstf_function_destruct(lstf_codenode *node)
{
    lstf_function *function = (lstf_function *)node;

    ptr_list_destroy(function->parameters);
    lstf_codenode_unref(function->return_type);
    lstf_codenode_unref(function->block);
    lstf_codenode_unref(function->scope);
}

const lstf_codenode_vtable function_vtable = {
    lstf_function_accept,
    lstf_function_accept_children,
    lstf_function_destruct
};

lstf_symbol *lstf_function_new(const lstf_sourceref *source_reference, 
                               char                 *name,
                               bool                  is_builtin, 
                               bool                  is_async)
{
    lstf_function *function = calloc(1, sizeof *function);

    lstf_symbol_construct(lstf_symbol_cast(function),
            &function_vtable,
            source_reference,
            lstf_symbol_type_function,
            name);

    function->parameters = ptr_list_new((collection_item_ref_func) lstf_codenode_ref, 
            (collection_item_unref_func) lstf_codenode_unref);
    function->scope = lstf_scope_new((lstf_codenode *)function);
    function->is_async = is_async;
    function->is_builtin = is_builtin;

    if (!function->is_builtin) {
        function->block = lstf_block_new();
        lstf_codenode_set_parent(function->block, function);
    }

    return lstf_symbol_cast(function);
}

void lstf_function_add_parameter(lstf_function *function, lstf_variable *variable)
{
    ptr_list_append(function->parameters, variable);
    lstf_scope_add_symbol(function->scope, lstf_symbol_cast(variable));
}
