#pragma once

#include "lstf-sourceref.h"
#include "lstf-datatype.h"

struct _lstf_patterntype {
    lstf_datatype parent_struct;
};
typedef struct _lstf_patterntype lstf_patterntype;

lstf_datatype *lstf_patterntype_new(const lstf_sourceref *source_reference);
