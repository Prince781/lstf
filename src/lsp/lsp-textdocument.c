#include "lsp-textdocument.h"
#include "data-structures/string-builder.h"
#include "json/json-serializable.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

json_serializable_impl_as_object(
    lsp_textdocument, "uri", "languageId", "version", "text");

static json_serialization_status lsp_textdocument_deserialize_property(lsp_textdocument *doc,
                                                                       const char       *property_name,
                                                                       json_node        *property_node)
{
    if (strcmp(property_name, "uri") == 0) {
        json_string *jstr = json_node_cast(property_node, string);
        if (!jstr)
            return json_serialization_status_invalid_type;
        if (!(doc->uri = strdup(jstr->value))) {
            fprintf(stderr,
                    "error: failed to dup string for property `%s': %s\n",
                    property_name, strerror(errno));
            abort();
        }
    } else if (strcmp(property_name, "languageId") == 0) {
        json_string *jstr = json_node_cast(property_node, string);
        if (!jstr)
            return json_serialization_status_invalid_type;
        if (!(doc->language_id = strdup(jstr->value))) {
            fprintf(stderr,
                    "error: failed to dup string for property `%s': %s\n",
                    property_name, strerror(errno));
            abort();
        }
    } else if (strcmp(property_name, "version") == 0) {
        json_integer *jint = json_node_cast(property_node, integer);
        if (!jint)
            return json_serialization_status_invalid_type;
        doc->version = jint->value;
    } else if (strcmp(property_name, "text") == 0) {
        json_string *jstr = json_node_cast(property_node, string);
        if (!jstr)
            return json_serialization_status_invalid_type;
        doc->text = string_ref(string_new_copy_data(jstr->value));
    } else {
        json_serializable_fail_with_unhandled_property(property_name);
    }
    return json_serialization_status_continue;
}

static json_serialization_status lsp_textdocument_serialize_property(lsp_textdocument const *doc,
                                                                     const char             *property_name,
                                                                     json_node             **property_node)
{
    if (strcmp(property_name, "uri") == 0) {
        *property_node = json_string_new(doc->uri);
    } else if (strcmp(property_name, "languageId") == 0) {
        *property_node = json_string_new(doc->language_id);
    } else if (strcmp(property_name, "version") == 0) {
        *property_node = json_integer_new(doc->version);
    } else if (strcmp(property_name, "text") == 0) {
        *property_node = json_string_new(doc->text->buffer);
    } else {
        json_serializable_fail_with_unhandled_property(property_name);
    }
    return json_serialization_status_continue;
}

json_serializable_impl_as_enum(lsp_textdocumentsynckind,
                               lsp_textdocumentsynckind_none,
                               lsp_textdocumentsynckind_full,
                               lsp_textdocumentsynckind_incremental);

void lsp_document_dtor(lsp_textdocument *doc)
{
    free(doc->uri);
    doc->uri = NULL;
    string_unref(doc->text);
    doc->text = NULL;
    // TODO: handle doc->language_id
}
