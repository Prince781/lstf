#include "lsp-textdocument.h"
#include "data-structures/string-builder.h"
#include <stdlib.h>
#include <string.h>

void lsp_document_ctor(lsp_textdocument *doc, const char *uri, const char *content)
{
    doc->uri = strdup(uri);
    doc->content = string_ref(string_new_copy_data(content));
}

void lsp_document_dtor(lsp_textdocument *doc)
{
    free(doc->uri);
    doc->uri = NULL;
    string_unref(doc->content);
    doc->content = NULL;
}
