#pragma once

#include "lstf-codenode.h"
#include "lstf-symbol.h"

struct _lstf_variable {
    lstf_symbol parent_struct;
};
typedef struct _lstf_variable lstf_variable;

lstf_variable *lstf_variable_new(const lstf_sourceref *source_reference, char *name);
