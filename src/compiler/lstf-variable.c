#include "lstf-variable.h"
#include "compiler/lstf-datatype.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-symbol.h"
#include <stdlib.h>
#include <string.h>

static void lstf_variable_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_variable(visitor, (lstf_variable *)code_node);
}

static void lstf_variable_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_variable *variable = (lstf_variable *)code_node;
    if (variable->variable_type)
        lstf_codenode_accept(variable->variable_type, visitor);
}

static void lstf_variable_destruct(lstf_codenode *code_node)
{
    lstf_variable *variable = (lstf_variable *)code_node;

    lstf_codenode_unref(variable->variable_type);
    variable->variable_type = NULL;
    lstf_symbol_destruct(code_node);
}

static const lstf_codenode_vtable variable_vtable = {
    lstf_variable_accept,
    lstf_variable_accept_children,
    lstf_variable_destruct
};

lstf_symbol *lstf_variable_new(const lstf_sourceref *source_reference,
                               const char           *name,
                               bool                  is_builtin)
{
    lstf_variable *variable = calloc(1, sizeof *variable);

    lstf_symbol_construct((lstf_symbol *)variable, 
            &variable_vtable,
            source_reference,
            lstf_symbol_type_variable,
            strdup(name),
            is_builtin);

    return (lstf_symbol *)variable;
}

void lstf_variable_set_variable_type(lstf_variable *variable, lstf_datatype *data_type)
{
    lstf_codenode_unref(variable->variable_type);

    if (((lstf_codenode *)data_type)->parent_node) {
        // duplicate
        variable->variable_type = lstf_codenode_ref(lstf_datatype_copy(data_type));
    } else {
        variable->variable_type = lstf_codenode_ref(data_type);
    }

    lstf_codenode_set_parent(variable->variable_type, variable);
}
