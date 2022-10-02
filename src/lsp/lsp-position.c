#include "lsp-position.h"
#include "data-structures/iterator.h"
#include "util.h"
#include "json/json.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

json_serializable_impl_as_object(lsp_position, "line", "character");

static json_serialization_status lsp_position_deserialize_property(lsp_position *self,
                                                                   const char   *property_name,
                                                                   json_node    *property_node)
{
    if (strcmp(property_name, "line") == 0) {
        if (property_node->node_type != json_node_type_integer)
            return json_serialization_status_invalid_type;
        self->line = ((json_integer *)property_node)->value;
    } else if (strcmp(property_name, "character") == 0) {
        if (property_node->node_type != json_node_type_integer)
            return json_serialization_status_invalid_type;
        self->character = ((json_integer *)property_node)->value;
    } else {
        // ignore the unrecognized property
    }

    return json_serialization_status_continue;
}

static json_serialization_status lsp_position_serialize_property(const lsp_position *self,
                                                                 const char         *property_name,
                                                                 json_node         **property_node)
{
    if (strcmp(property_name, "line") == 0)
        *property_node = json_integer_new(self->line);
    else if (strcmp(property_name, "character") == 0)
        *property_node = json_integer_new(self->character);
    else
        json_serializable_fail_with_unhandled_property(property_name);

    return json_serialization_status_continue;
}
