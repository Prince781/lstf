#pragma once

#include "lstf-sourceref.h"

void lstf_report_valist(const lstf_sourceref *source_ref, const char *domain, const char *message, ...);

#define lstf_report_error(source_ref, ...) lstf_report_valist(source_ref, "error", __VA_ARGS__)
