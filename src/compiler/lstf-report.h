#pragma once

#include "lstf-sourceref.h"

enum _lstf_report_domain {
    lstf_report_domain_error,
    lstf_report_domain_warning,
    lstf_report_domain_info,
};
typedef enum _lstf_report_domain lstf_report_domain;

void lstf_report(const lstf_sourceref *source_ref, lstf_report_domain domain, const char *message, ...)
    __attribute__((format (printf, 3, 4)));

#define lstf_report_error(source_ref, ...) lstf_report(source_ref, lstf_report_domain_error, __VA_ARGS__)

#define lstf_report_warning(source_ref, ...) lstf_report(source_ref, lstf_report_domain_warning, __VA_ARGS__)

#define lstf_report_info(source_ref, ...) lstf_report(source_ref, lstf_report_domain_info, __VA_ARGS__)
