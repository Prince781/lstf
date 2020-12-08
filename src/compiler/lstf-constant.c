#include "lstf-constant.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-symbol.h"
#include <string.h>
#include <stdlib.h>

static void lstf_constant_accept(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_constant(visitor, (lstf_constant *)node);
}

static void lstf_constant_accept_children(lstf_codenode *node, lstf_codevisitor *visitor)
{
    lstf_constant *constant = (lstf_constant *)node;

    lstf_codenode_accept(constant->expression, visitor);
}

static void lstf_constant_destruct(lstf_codenode *code_node)
{
    lstf_constant *constant = (lstf_constant *)code_node;

    lstf_codenode_unref(constant->expression);
    constant->expression = NULL;

    lstf_symbol_destruct(code_node);
}

static const lstf_codenode_vtable constant_vtable = {
    lstf_constant_accept,
    lstf_constant_accept_children,
    lstf_constant_destruct
};

lstf_symbol *lstf_constant_new(const lstf_sourceref *source_reference,
                               const char           *name,
                               lstf_expression      *expression)
{
    lstf_constant *constant = calloc(1, sizeof *constant);

    lstf_symbol_construct((lstf_symbol *)constant,
            &constant_vtable,
            source_reference,
            lstf_symbol_type_constant,
            strdup(name),
            false);

    constant->expression = lstf_codenode_ref(expression);
    return (lstf_symbol *)constant;
}
