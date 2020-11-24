#pragma once

#include "lstf-sourceref.h"

struct _lstf_parsererror {
    lstf_sourceref source_reference;
    char *message;
};
typedef struct _lstf_parsererror lstf_parsererror;

lstf_parsererror *lstf_parsererror_new(const lstf_sourceref *source_reference,
                                       const char           *format,
                                       ...)
    __attribute__((format (printf, 2, 3)));

void lstf_parsererror_destroy(lstf_parsererror *error);
