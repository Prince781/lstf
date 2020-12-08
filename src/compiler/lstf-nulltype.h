#pragma once

#include "lstf-sourceref.h"
#include "lstf-datatype.h"

struct _lstf_nulltype {
    lstf_datatype parent_struct;
};
typedef struct _lstf_nulltype lstf_nulltype;

lstf_datatype *lstf_nulltype_new(const lstf_sourceref *source_reference); 
