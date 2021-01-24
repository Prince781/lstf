#pragma once

#include "data-structures/ptr-list.h"
#include "lstf-sourceref.h"

struct _lstf_parsererror {
    lstf_sourceref source_reference;
    char *message;

    /**
     * Contains a list of `(lstf_parsernote *)`
     */
    ptr_list *notes;
};
typedef struct _lstf_parsererror lstf_parsererror;

struct _lstf_parsernote {
    lstf_sourceref source_reference;
    char *message;
};
typedef struct _lstf_parsernote lstf_parsernote;

lstf_parsererror *lstf_parsererror_new(const lstf_sourceref *source_reference,
                                       const char           *format,
                                       ...)
    __attribute__((format (printf, 2, 3)));

void lstf_parsererror_add_note(lstf_parsererror     *error,
                               const lstf_sourceref *source_reference,
                               const char           *format,
                               ...)
    __attribute__((format (printf, 3, 4)));

void lstf_parsererror_destroy(lstf_parsererror *error);
