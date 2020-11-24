#include "lstf-symbol.h"
#include "compiler/lstf-codenode.h"
#include <stdlib.h>

void lstf_symbol_construct(lstf_symbol              *symbol,
                           const lstf_sourceref     *source_reference,
                           lstf_codenode_dtor_func   dtor_func,
                           lstf_symbol_type          symbol_type,
                           char                     *name)
{
    lstf_codenode_construct((lstf_codenode *)symbol,
            lstf_codenode_type_symbol,
            source_reference, 
            dtor_func);

    symbol->symbol_type = symbol_type;
    symbol->name = name;
}

void lstf_symbol_destruct(lstf_codenode *code_node)
{
    lstf_symbol *symbol = (lstf_symbol *)code_node;
    free(symbol->name);
    symbol->name = NULL;
}
