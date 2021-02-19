#pragma once

#include "lstf-codenode.h"

enum _lstf_statement_type {
    lstf_statement_type_assignment,
    lstf_statement_type_patterntest,

    /**
     * An expression statement.
     */
    lstf_statement_type_expression,

    /**
     * A declaration for a type or function.
     */
    lstf_statement_type_declaration,

    /**
     * A return statement
     */
    lstf_statement_type_return,

    lstf_statement_type_ifstatement,
};
typedef enum _lstf_statement_type lstf_statement_type;

struct _lstf_statement {
    lstf_codenode parent_struct;
    lstf_statement_type stmt_type;
};
typedef struct _lstf_statement lstf_statement;

static inline lstf_statement *lstf_statement_cast(void *node)
{
    lstf_codenode *code_node = lstf_codenode_cast(node);

    if (code_node && code_node->codenode_type == lstf_codenode_type_statement) {
        switch (((lstf_statement *)code_node)->stmt_type) {
        case lstf_statement_type_assignment:
        case lstf_statement_type_declaration:
        case lstf_statement_type_expression:
        case lstf_statement_type_ifstatement:
        case lstf_statement_type_patterntest:
        case lstf_statement_type_return:
            return node;
        }
    }

    return NULL;
}

void lstf_statement_construct(lstf_statement             *stmt,
                              const lstf_codenode_vtable *vtable,
                              const lstf_sourceref       *source_reference,
                              lstf_statement_type         stmt_type)
    __attribute__((nonnull (1, 2)));
