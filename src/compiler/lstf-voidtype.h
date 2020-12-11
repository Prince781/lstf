#pragma once

#include "lstf-sourceref.h"
#include "lstf-datatype.h"

struct _lstf_voidtype {
    lstf_datatype parent_struct;
};
typedef struct _lstf_voidtype lstf_voidtype;

lstf_datatype *lstf_voidtype_new(const lstf_sourceref *source_reference);
