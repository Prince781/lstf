#pragma once

#include "lstf-sourceloc.h"
#include "lstf-file.h"

struct _lstf_sourceref {
    const lstf_file *file;
    lstf_sourceloc begin;
    lstf_sourceloc end;
};
typedef struct _lstf_sourceref lstf_sourceref;

#define lstf_sourceref_at_location(file, begin) (lstf_sourceref){file, begin, begin}
