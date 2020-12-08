#pragma once

#include "lstf-sourceloc.h"
#include "lstf-file.h"
#include <stddef.h>

struct _lstf_sourceref {
    const lstf_file *file;
    lstf_sourceloc begin;
    lstf_sourceloc end;
};
typedef struct _lstf_sourceref lstf_sourceref;

#define lstf_sourceref_at_location(file, begin) (lstf_sourceref){file, begin, begin}

#define lstf_sourceref_default_from_file(file) lstf_sourceref_at_location(file, ((lstf_sourceloc){0, 0, NULL}))

char *lstf_sourceref_get_string(lstf_sourceloc begin, lstf_sourceloc end);
