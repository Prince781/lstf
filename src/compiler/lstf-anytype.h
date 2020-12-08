#pragma once

#include "lstf-sourceref.h"
#include "lstf-datatype.h"

struct _lstf_anytype {
    lstf_datatype parent_struct;
};
typedef struct _lstf_anytype lstf_anytype;

lstf_datatype *lstf_anytype_new(const lstf_sourceref *source_reference);
