#pragma once

#include "lstf-expression.h"
#include "lstf-datatype.h"
#include "lstf-codenode.h"
#include "lstf-symbol.h"

struct _lstf_variable {
    lstf_symbol parent_struct;
    lstf_datatype *variable_type;
    lstf_expression *initializer;
};
typedef struct _lstf_variable lstf_variable;

static inline lstf_variable *lstf_variable_cast(void *node)
{
    lstf_codenode *code_node = node;

    if (code_node && code_node->codenode_type == lstf_codenode_type_symbol &&
            ((lstf_symbol *)code_node)->symbol_type == lstf_symbol_type_variable)
        return (lstf_variable *)code_node;
    return NULL;
}

lstf_symbol *lstf_variable_new(const lstf_sourceref *source_reference,
                               const char           *name,
                               lstf_datatype        *variable_type,
                               lstf_expression      *initializer,
                               bool                  is_builtin);

void lstf_variable_set_variable_type(lstf_variable *variable, lstf_datatype *data_type);
