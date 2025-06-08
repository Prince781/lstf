#include "lsp-window.h"
#include "json/json-serializable.h"

json_serializable_impl_as_enum(lsp_message_type,
                               lsp_message_type_error,
                               lsp_message_type_warning,
                               lsp_message_type_info,
                               lsp_message_type_log);

json_serializable_impl_as_object(lsp_showmessageparams, "type", "message");


static json_serialization_status
lsp_showmessageparams_deserialize_property(lsp_showmessageparams *self,
                                           const char            *property_name,
                                           json_node             *property_node)
{
    if (strcmp(property_name, "type") == 0) {
        return json_deserialize(lsp_message_type, &self->type, property_node);
    } else if (strcmp(property_name, "message") == 0) {
        json_string *jstr = json_node_cast(property_node, string);
        if (!jstr)
            return json_serialization_status_invalid_type;
        self->message = strdup(jstr->value);
    } else {
        // ignore unknown property
    }

    return json_serialization_status_continue;
}

static json_serialization_status
lsp_showmessageparams_serialize_property(const lsp_showmessageparams *self,
                                         const char *property_name,
                                         json_node **property_node)
{
    (void) self;
    (void) property_node;
    json_serializable_fail_with_unhandled_property(property_name);
}
