#pragma once

#include "data-structures/string-builder.h"
#include "json/json-serializable.h"

json_serializable_decl_as_object(lsp_textdocument, {
    /**
     * Fully unescaped URI
     */
    char *uri;

    /**
     * Full content of the text document.
     */
    string *text;
});

/**
 * `enum TextDocumentSyncKind`
 */
typedef enum {
    lsp_textdocumentsynckind_none,
    lsp_textdocumentsynckind_full,
    lsp_textdocumentsynckind_incremental
} lsp_textdocumentsynckind;

void lsp_document_dtor(lsp_textdocument *doc);
