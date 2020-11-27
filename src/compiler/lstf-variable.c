#include "lstf-variable.h"
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
    (void)code_node;
    (void)visitor;
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

lstf_symbol *lstf_variable_new(const lstf_sourceref *source_reference, const char *name)
{
    lstf_variable *variable = calloc(1, sizeof *variable);

    lstf_symbol_construct((lstf_symbol *)variable, 
            &variable_vtable,
            source_reference,
            lstf_symbol_type_variable,
            strdup(name));

    return (lstf_symbol *)variable;
}
