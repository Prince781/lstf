#pragma once

#include "lstf-sourceref.h"

void lstf_report(const lstf_sourceref *source_ref, const char *domain, const char *message, ...)
    __attribute__((format (printf, 3, 4)));

#define lstf_report_error(source_ref, ...) lstf_report(source_ref, "error", __VA_ARGS__)
