#pragma once

#include "lstf-datatype.h"
#include "lstf-sourceref.h"
#include "lstf-literal.h"
#include "lstf-symbol.h"

struct _lstf_constant {
    lstf_symbol parent_struct; 
    lstf_expression *expression;
};
typedef struct _lstf_constant lstf_constant;

static inline lstf_constant *lstf_constant_cast(void *node)
{
    lstf_symbol *symbol = lstf_symbol_cast(node);

    if (symbol && symbol->symbol_type == lstf_symbol_type_constant)
        return node;
    return NULL;
}

lstf_symbol *lstf_constant_new(const lstf_sourceref *source_reference,
                               const char           *name,
                               lstf_expression      *expression);

lstf_datatype *lstf_constant_get_value_type(lstf_constant *self);
