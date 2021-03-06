#include "lstf-symbol.h"
#include "lstf-codenode.h"
#include <stdlib.h>

void lstf_symbol_construct(lstf_symbol                *symbol,
                           const lstf_codenode_vtable *vtable,
                           const lstf_sourceref       *source_reference,
                           lstf_symbol_type            symbol_type,
                           char                       *name,
                           bool                        is_builtin)
{
    lstf_codenode_construct((lstf_codenode *)symbol,
            vtable,
            lstf_codenode_type_symbol,
            source_reference);

    symbol->symbol_type = symbol_type;
    symbol->name = name;
    symbol->is_builtin = is_builtin;
}

void lstf_symbol_destruct(lstf_codenode *code_node)
{
    lstf_symbol *symbol = (lstf_symbol *)code_node;
    free(symbol->name);
    symbol->name = NULL;
}
