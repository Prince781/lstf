#include "lstf-variable.h"
#include "lstf-symbol.h"
#include <stdlib.h>
#include <string.h>

lstf_variable *lstf_variable_new(const lstf_sourceref *source_reference, char *name)
{
    lstf_variable *variable = calloc(1, sizeof *variable);

    lstf_symbol_construct((lstf_symbol *)variable, 
            source_reference, lstf_symbol_destruct,
            lstf_symbol_type_variable,
            name);

    return variable;
}
