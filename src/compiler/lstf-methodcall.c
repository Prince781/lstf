#include "lstf-methodcall.h"
#include "compiler/lstf-codenode.h"
#include "compiler/lstf-expression.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include <stdlib.h>

void lstf_methodcall_destruct(lstf_codenode *code_node)
{
    lstf_methodcall *expr = (lstf_methodcall *)code_node;

    lstf_codenode_unref(expr->call);
    ptr_list_destroy(expr->arguments);
}

lstf_expression *lstf_methodcall_new(const lstf_sourceref *source_reference,
                                     lstf_expression      *call,
                                     ptr_list             *arguments)
{
    lstf_methodcall *expr = calloc(1, sizeof *expr);

    lstf_expression_construct((lstf_expression *)expr, 
            source_reference,
            lstf_methodcall_destruct,
            lstf_expression_type_methodcall);

    expr->call = lstf_codenode_ref(call);
    expr->arguments = arguments;

    for (iterator it = ptr_list_iterator_create(expr->arguments); it.has_next; it = iterator_next(it))
        lstf_codenode_set_parent(iterator_get_item(it), expr);

    return (lstf_expression *)expr;
}
