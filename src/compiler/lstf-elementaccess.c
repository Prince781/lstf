#include "lstf-elementaccess.h"
#include "lstf-codenode.h"
#include "lstf-codevisitor.h"
#include "lstf-expression.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include <stdlib.h>

static void lstf_elementaccess_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_element_access(visitor, (lstf_elementaccess *)code_node);
    lstf_codevisitor_visit_expression(visitor, lstf_expression_cast(code_node));
}

static void lstf_elementaccess_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_elementaccess *access = (lstf_elementaccess *)code_node;

    lstf_codenode_accept(access->inner, visitor);
    for (iterator it = ptr_list_iterator_create(access->arguments); it.has_next; it = iterator_next(it))
        lstf_codenode_accept(iterator_get_item(it), visitor);
}

static void lstf_elementaccess_destruct(lstf_codenode *code_node)
{
    lstf_elementaccess *expr = (lstf_elementaccess *)code_node;

    ptr_list_destroy(expr->arguments);
    expr->arguments = NULL;
    lstf_codenode_unref(expr->inner);
    expr->inner = NULL;
    lstf_expression_destruct(code_node);
}

static const lstf_codenode_vtable elementaccess_vtable = {
    lstf_elementaccess_accept,
    lstf_elementaccess_accept_children,
    lstf_elementaccess_destruct
};

lstf_expression *lstf_elementaccess_new(const lstf_sourceref *source_reference,
                                        lstf_expression      *inner,
                                        ptr_list             *arguments)
{
    lstf_elementaccess *expr = calloc(1, sizeof *expr);

    lstf_expression_construct((lstf_expression *)expr,
            &elementaccess_vtable,
            source_reference,
            lstf_expression_type_elementaccess);

    expr->inner = lstf_codenode_ref(inner);
    expr->arguments = arguments;

    for (iterator it = ptr_list_iterator_create(expr->arguments); it.has_next; it = iterator_next(it))
        lstf_codenode_set_parent(iterator_get_item(it), expr);

    return (lstf_expression *)expr;
}
