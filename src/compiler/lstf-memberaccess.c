#include "lstf-memberaccess.h"
#include "compiler/lstf-codenode.h"
#include "compiler/lstf-expression.h"
#include <stdlib.h>

void lstf_memberaccess_destruct(lstf_codenode *code_node)
{
    lstf_memberaccess *ma = (lstf_memberaccess *)code_node;

    free(ma->member_name);
    ma->member_name = NULL;
}

lstf_expression *lstf_memberaccess_new(const lstf_sourceref *source_reference,
                                       lstf_expression *inner,
                                       char *member_name)
{
    lstf_memberaccess *ma = calloc(1, sizeof *ma);

    lstf_expression_construct((lstf_expression *)ma,
            source_reference, 
            lstf_memberaccess_destruct,
            lstf_expression_type_memberaccess);

    ma->inner = lstf_codenode_ref(inner);
    ma->member_name = member_name;

    return (lstf_expression *) ma;
}
