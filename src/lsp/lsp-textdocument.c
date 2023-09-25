#include "lsp-textdocument.h"
#include "data-structures/string-builder.h"
#include "json/json-serializable.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

json_serializable_impl_as_object(lsp_textdocument, "uri", "text");

static json_serialization_status lsp_textdocument_deserialize_property(lsp_textdocument *doc,
                                                                       const char       *property_name,
                                                                       json_node        *property_node)
{
    if (strcmp(property_name, "uri") == 0) {
        if (property_node->node_type != json_node_type_string)
            return json_serialization_status_invalid_type;
        if (!(doc->uri = strdup(((json_string *)property_node)->value))) {
            fprintf(stderr, "warning: failed to dup string for property `%s': %s\n",
                    property_name, strerror(errno));
        }
    } else if (strcmp(property_name, "text") == 0) {
        if (property_node->node_type != json_node_type_string)
            return json_serialization_status_invalid_type;
        doc->text = string_ref(string_new_copy_data(((json_string *)property_node)->value));
    } else {
        // ignore unhandled property
    }
    return json_serialization_status_continue;
}

static json_serialization_status lsp_textdocument_serialize_property(lsp_textdocument const *doc,
                                                                     const char             *property_name,
                                                                     json_node             **property_node)
{
    if (strcmp(property_name, "uri") == 0) {
        *property_node = json_string_new(doc->uri);
    } else if (strcmp(property_name, "text") == 0) {
        *property_node = json_string_new(doc->text->buffer);
    } else {
        json_serializable_fail_with_unhandled_property(property_name);
    }
    return json_serialization_status_continue;
}

void lsp_document_dtor(lsp_textdocument *doc)
{
    free(doc->uri);
    doc->uri = NULL;
    string_unref(doc->text);
    doc->text = NULL;
}
