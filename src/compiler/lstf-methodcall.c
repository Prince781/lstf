#include "lstf-methodcall.h"
#include "compiler/lstf-codenode.h"
#include "compiler/lstf-codevisitor.h"
#include "compiler/lstf-expression.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include <stdlib.h>

static void lstf_methodcall_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_method_call(visitor, (lstf_methodcall *)code_node);
    lstf_codevisitor_visit_expression(visitor, lstf_expression_cast(code_node));
}

static void lstf_methodcall_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_methodcall *mcall = (lstf_methodcall *)code_node;

    lstf_codenode_accept(mcall->call, visitor);
    for (iterator it = ptr_list_iterator_create(mcall->arguments); it.has_next; it = iterator_next(it))
        lstf_codenode_accept(iterator_get_item(it), visitor);
}

static void lstf_methodcall_destruct(lstf_codenode *code_node)
{
    lstf_methodcall *expr = (lstf_methodcall *)code_node;

    lstf_codenode_unref(expr->call);
    ptr_list_destroy(expr->arguments);
    lstf_expression_destruct(code_node);
}

static const lstf_codenode_vtable methodcall_vtable = {
    lstf_methodcall_accept,
    lstf_methodcall_accept_children,
    lstf_methodcall_destruct
};

lstf_expression *lstf_methodcall_new(const lstf_sourceref *source_reference,
                                     lstf_expression      *call,
                                     ptr_list             *arguments)
{
    lstf_methodcall *expr = calloc(1, sizeof *expr);

    lstf_expression_construct((lstf_expression *)expr, 
            &methodcall_vtable,
            source_reference,
            lstf_expression_type_methodcall);

    expr->call = lstf_codenode_ref(call);
    expr->arguments = arguments;

    for (iterator it = ptr_list_iterator_create(expr->arguments); it.has_next; it = iterator_next(it))
        lstf_codenode_set_parent(iterator_get_item(it), expr);

    return (lstf_expression *)expr;
}
