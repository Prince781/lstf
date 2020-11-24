#include "lstf-literal.h"
#include "lstf-codenode.h"
#include "lstf-expression.h"
#include <stdlib.h>

void lstf_literal_destruct(lstf_codenode *code_node)
{
    lstf_literal *lit = (lstf_literal *)code_node;

    if (lit->literal_type == lstf_literal_type_string) {
        free(lit->value.string_value);
        lit->value.string_value = NULL;
    }
}

lstf_expression *lstf_literal_new(const lstf_sourceref *source_reference,
                                  lstf_literal_type     literal_type,
                                  lstf_literal_value    literal_value)
{
    lstf_literal *lit = calloc(1, sizeof *lit);

    lstf_expression_construct((lstf_expression *)lit,
            source_reference,
            lstf_literal_destruct,
            lstf_expression_type_literal);

    lit->literal_type = literal_type;
    lit->value = literal_value;

    return (lstf_expression *)lit;
}
