#pragma once

#include "json/json-serializable.h"
#include "data-structures/array.h"
#include "lsp-range.h"

enum _lsp_diagnosticseverity {
    lsp_diagnosticseverity__unset,
    lsp_diagnosticseverity_error,
    lsp_diagnosticseverity_warning,
    lsp_diagnosticseverity_information,
    lsp_diagnosticseverity_hint,
    lsp_diagnosticseverity__numvalues
};
typedef enum _lsp_diagnosticseverity lsp_diagnosticseverity;

enum _lsp_diagnostictag {
    lsp_diagnostictag__unset,
    lsp_diagnostictag_unnecessary,
    lsp_diagnostictag_deprecated,
    lsp_diagnostictag__numvalues
};
typedef enum _lsp_diagnostictag lsp_diagnostictag;

json_serializable_decl_as_object(lsp_diagnostic, {
    lsp_range range;

    /**
     * (optional)
     */
    lsp_diagnosticseverity severity;

    /**
     * (optional)
     */
    char *code;

    /**
     * (optional)
     */
    char *source;

    char *message;

    /**
     * (optional)
     */
    array(lsp_diagnostictag) tags;
});

static inline void 
lsp_diagnostic_clear(lsp_diagnostic *diag)
{
    if (diag->code)
        free(diag->code);
    if (diag->source)
        free(diag->source);
    if (diag->message)
        free(diag->message);
    array_destroy(&diag->tags);
}
