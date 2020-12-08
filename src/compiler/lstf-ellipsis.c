#include "lstf-ellipsis.h"
#include "compiler/lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-expression.h"
#include <stdlib.h>

static void lstf_ellipsis_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_ellipsis(visitor, (lstf_ellipsis *)code_node);
    lstf_codevisitor_visit_expression(visitor, lstf_expression_cast(code_node));
}

static void lstf_ellipsis_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    (void) code_node;
    (void) visitor;
}

static void lstf_ellipsis_destruct(lstf_codenode *code_node)
{
    lstf_expression_destruct(code_node);
}

static const lstf_codenode_vtable ellipsis_vtable = {
    lstf_ellipsis_accept,
    lstf_ellipsis_accept_children,
    lstf_ellipsis_destruct
};

lstf_expression *lstf_ellipsis_new(const lstf_sourceref *source_reference)
{
    lstf_ellipsis *ellipsis = calloc(1, sizeof *ellipsis);

    lstf_expression_construct(
        (lstf_expression *)ellipsis,
        &ellipsis_vtable,
        source_reference,
        lstf_expression_type_ellipsis);

    return (lstf_expression *) ellipsis;
}
