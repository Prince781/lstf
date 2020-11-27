#pragma once

#include "lstf-variable.h"
#include "lstf-sourceref.h"
#include "lstf-file.h"
#include "data-structures/ptr-list.h"
#include "lstf-datatype.h"
#include "lstf-symbol.h"
#include "lstf-scope.h"
#include <stdbool.h>

struct _lstf_function {
    lstf_symbol parent_struct;
    lstf_datatype *return_type;
    ptr_list *parameters;

    /**
     * Contains the parameters of this function.
     */
    lstf_scope *scope;

    /**
     * If NULL, then this is a native/builtin function.
     */
    lstf_block *block;

    bool is_builtin;

    /**
     * If true, then calls to this function can use the `await` keyword.
     */
    bool is_async;
};
typedef struct _lstf_function lstf_function;

static inline lstf_function *lstf_function_cast(void *node)
{
    lstf_codenode *code_node = node;

    if (code_node && code_node->codenode_type == lstf_codenode_type_symbol &&
            ((lstf_symbol *)code_node)->symbol_type == lstf_symbol_type_function)
        return (lstf_function *)code_node;
    return NULL;
}

lstf_symbol *lstf_function_new(const lstf_sourceref *source_reference, 
                               char                 *name,
                               bool                  is_builtin, 
                               bool                  is_async);

void lstf_function_add_parameter(lstf_function *function, lstf_variable *variable);
