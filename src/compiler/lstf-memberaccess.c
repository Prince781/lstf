#include "lstf-memberaccess.h"
#include "compiler/lstf-codenode.h"
#include "compiler/lstf-codevisitor.h"
#include "compiler/lstf-expression.h"
#include <string.h>
#include <stdlib.h>

static void lstf_memberaccess_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_member_access(visitor, (lstf_memberaccess *)code_node);
    lstf_codevisitor_visit_expression(visitor, lstf_expression_cast(code_node));
}

static void lstf_memberaccess_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_memberaccess *expr = (lstf_memberaccess *)code_node;

    if (expr->inner)
        lstf_codenode_accept(expr->inner, visitor);
}

static void lstf_memberaccess_destruct(lstf_codenode *code_node)
{
    lstf_memberaccess *ma = (lstf_memberaccess *)code_node;

    lstf_codenode_unref(ma->inner);
    free(ma->member_name);
    ma->member_name = NULL;
    lstf_expression_destruct(code_node);
}

static const lstf_codenode_vtable memberaccess_vtable = {
    lstf_memberaccess_accept,
    lstf_memberaccess_accept_children,
    lstf_memberaccess_destruct
};

lstf_expression *lstf_memberaccess_new(const lstf_sourceref *source_reference,
                                       lstf_expression *inner,
                                       const char *member_name)
{
    lstf_memberaccess *ma = calloc(1, sizeof *ma);

    lstf_expression_construct((lstf_expression *)ma,
            &memberaccess_vtable,
            source_reference, 
            lstf_expression_type_memberaccess);

    ma->inner = lstf_codenode_ref(inner);
    ma->member_name = strdup(member_name);

    return (lstf_expression *) ma;
}
