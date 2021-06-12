#pragma once

#include "data-structures/string-builder.h"

typedef struct {
    /**
     * Fully unescaped URI
     */
    char *uri;

    /**
     * Full content of the text document.
     */
    string *content;
} lsp_textdocument;

/**
 * `enum TextDocumentSyncKind`
 */
typedef enum {
    lsp_textdocumentsynckind_none,
    lsp_textdocumentsynckind_full,
    lsp_textdocumentsynckind_incremental
} lsp_textdocumentsynckind;

/**
 * Fills in the text document with `uri` and `content`
 */
void lsp_document_ctor(lsp_textdocument *doc, const char *uri, const char *content);

void lsp_document_dtor(lsp_textdocument *doc);
