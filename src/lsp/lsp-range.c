#include "lsp-range.h"
#include "lsp/lsp-position.h"
#include "json/json-serializable.h"
#include <string.h>

json_serializable_impl_as_object(lsp_range, "start", "end");

static json_serialization_status lsp_range_deserialize_property(lsp_range  *self,
                                                                const char *property_name,
                                                                json_node  *property_node)
{
    if (strcmp(property_name, "start") == 0)
        return json_deserialize(lsp_position, &self->start, property_node);
    else if (strcmp(property_name, "end") == 0)
        return json_deserialize(lsp_position, &self->end, property_node);

    // ignore unhandled property
    return json_serialization_status_continue;
}

static json_serialization_status lsp_range_serialize_property(lsp_range  *self,
                                                              const char *property_name,
                                                              json_node **property_node)
{
    if (strcmp(property_name, "start") == 0)
        return json_serialize(lsp_position, &self->start, property_node);
    else if (strcmp(property_name, "end") == 0)
        return json_serialize(lsp_position, &self->end, property_node);
    else
        json_serializable_fail_with_unhandled_property(property_name);
}
