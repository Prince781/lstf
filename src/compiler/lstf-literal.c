#include "lstf-literal.h"
#include "lstf-booleantype.h"
#include "lstf-doubletype.h"
#include "lstf-integertype.h"
#include "lstf-stringtype.h"
#include "lstf-anytype.h"
#include "lstf-codevisitor.h"
#include "lstf-codenode.h"
#include "lstf-expression.h"
#include <stdlib.h>

static void lstf_literal_accept(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    lstf_codevisitor_visit_literal(visitor, (lstf_literal *)code_node);
    lstf_codevisitor_visit_expression(visitor, lstf_expression_cast(code_node));
}

static void lstf_literal_accept_children(lstf_codenode *code_node, lstf_codevisitor *visitor)
{
    (void)code_node;
    (void)visitor;
}

static void lstf_literal_destruct(lstf_codenode *code_node)
{
    lstf_literal *lit = (lstf_literal *)code_node;

    if (lit->literal_type == lstf_literal_type_string) {
        free(lit->value.string_value);
        lit->value.string_value = NULL;
    }
    lstf_expression_destruct(code_node);
}

static const lstf_codenode_vtable literal_vtable = {
    lstf_literal_accept,
    lstf_literal_accept_children,
    lstf_literal_destruct
};

lstf_expression *lstf_literal_new(const lstf_sourceref *source_reference,
                                  lstf_literal_type     literal_type,
                                  lstf_literal_value    literal_value)
{
    lstf_literal *lit = calloc(1, sizeof *lit);

    lstf_expression_construct((lstf_expression *)lit,
            &literal_vtable,
            source_reference,
            lstf_expression_type_literal);

    lit->literal_type = literal_type;
    lit->value = literal_value;

    return (lstf_expression *)lit;
}
