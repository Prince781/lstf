#include "lstf-ellipsis.h"
#include "lstf-expression.h"
#include <stdlib.h>

void lstf_ellipsis_destruct(lstf_codenode *code_node)
{
    (void) code_node;
}

lstf_expression *lstf_ellipsis_new(const lstf_sourceref *source_reference)
{
    lstf_ellipsis *ellipsis = calloc(1, sizeof *ellipsis);

    lstf_expression_construct(
        (lstf_expression *)ellipsis,
        source_reference,
        lstf_ellipsis_destruct,
        lstf_expression_type_ellipsis);

    return (lstf_expression *) ellipsis;
}
