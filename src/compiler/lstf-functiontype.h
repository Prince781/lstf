#pragma once

#include "lstf-datatype.h"
#include "lstf-function.h"

struct _lstf_functiontype {
    lstf_datatype parent_struct;
};
typedef struct _lstf_functiontype lstf_functiontype;

void lstf_functiontype_new(lstf_function *function_symbol);
