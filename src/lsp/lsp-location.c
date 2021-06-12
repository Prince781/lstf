#include "lsp-location.h"
#include "lsp/lsp-range.h"
#include "json/json-serializable.h"
#include "json/json.h"
#include <string.h>

json_serializable_impl_as_object(lsp_location, "uri", "range");

static json_serialization_status lsp_location_deserialize_property(lsp_location *self,
                                                                   const char   *property_name,
                                                                   json_node    *property_node)
{
    if (strcmp(property_name, "uri") == 0) {
        if (property_node->node_type != json_node_type_string)
            return json_serialization_status_invalid_type;
        self->uri = strdup(((json_string *)property_node)->value);
        return json_serialization_status_continue;
    } else if (strcmp(property_name, "range") == 0) {
        return json_deserialize(lsp_range, &self->range, property_node);
    } else {
        // ignore unhandled property
    }

    return json_serialization_status_continue;
}

static json_serialization_status lsp_location_serialize_property(lsp_location *self,
                                                                 const char   *property_name,
                                                                 json_node   **property_node)
{
    if (strcmp(property_name, "uri") == 0) {
        *property_node = json_string_new(self->uri);
    } else if (strcmp(property_name, "range") == 0) {
        return json_serialize(lsp_range, &self->range, property_node);
    } else {
        json_serializable_fail_with_unhandled_property(property_name);
    }
    
    return json_serialization_status_continue;
}
