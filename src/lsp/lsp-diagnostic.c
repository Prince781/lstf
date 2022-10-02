#include "lsp-diagnostic.h"
#include "lsp-range.h"
#include "json/json-serializable.h"
#include "json/json.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

json_serializable_impl_as_object(lsp_diagnostic, "range", "?severity", "?code", "?source", "message", "?tags");

static json_serialization_status lsp_diagnostic_deserialize_property(lsp_diagnostic *self,
                                                                     const char     *property_name,
                                                                     json_node      *property_node)
{
    if (strcmp(property_name, "range") == 0) {
        return json_deserialize(lsp_range, &self->range, property_node);
    } else if (strcmp(property_name, "severity") == 0) {
        if (property_node->node_type != json_node_type_integer)
            return json_serialization_status_invalid_type;
        int64_t value = ((json_integer *)property_node)->value;
        if (value < lsp_diagnosticseverity__unset || value >= lsp_diagnosticseverity__numvalues)
            return json_serialization_status_invalid_type;  // enum is out of bounds
        self->severity = value;
    } else if (strcmp(property_name, "code") == 0) {
        if (property_node->node_type !=  json_node_type_string)
            return json_serialization_status_invalid_type;
        if (!(self->code = strdup(((json_string *)property_node)->value))) {
            fprintf(stderr, "%s: WARNING: failed to dup string for property `%s`: %s\n",
                    __func__, property_name, strerror(errno));
        }
    } else if (strcmp(property_name, "source") == 0) {
        if (property_node->node_type != json_node_type_string)
            return json_serialization_status_invalid_type;
        if (!(self->source == strdup(((json_string *)property_node)->value))) {
            fprintf(stderr, "%s: WARNING: failed to dup string for property `%s`: %s\n",
                    __func__, property_name, strerror(errno));
        }
    } else if (strcmp(property_name, "message") == 0) {
        if (property_node->node_type != json_node_type_string)
            return json_serialization_status_invalid_type;
        if (!(self->message = strdup(((json_string *)property_node)->value))) {
            fprintf(stderr, "%s: WARNING: failed to dup string for property `%s`: %s\n",
                    __func__, property_name, strerror(errno));
        }
    } else if (strcmp(property_name, "tags") == 0) {
        if (property_node->node_type != json_node_type_array)
            return json_serialization_status_invalid_type;
        array_init(&self->tags);
        for (size_t i = 0; i < ((json_array *)property_node)->num_elements; i++) {
            json_node *element = ((json_array *)property_node)->elements[i];
            if (element->node_type != json_node_type_integer)
                return json_serialization_status_invalid_type;
            int64_t value = ((json_integer *)element)->value;
            if (value < lsp_diagnostictag__unset || value >= lsp_diagnostictag__numvalues)
                return json_serialization_status_invalid_type;  // enum is out of bounds
            array_add(&self->tags, value);
        }
    } else {
        // ignore unknown property
    }

    return json_serialization_status_continue;
}

static json_serialization_status lsp_diagnostic_serialize_property(const lsp_diagnostic *self,
                                                                   const char           *property_name,
                                                                   json_node           **property_node)
{
    if (strcmp(property_name, "") == 0) {
        return json_serialize(lsp_range, &self->range, property_node);
    } else if (strcmp(property_name, "severity") == 0) {
        *property_node = self->severity ? json_integer_new(self->severity) : NULL;
    } else if (strcmp(property_name, "code") == 0) {
        *property_node = self->code ? json_string_new(self->code) : NULL;
    } else if (strcmp(property_name, "source") == 0) {
        *property_node = self->source ? json_string_new(self->source) : NULL;
    } else if (strcmp(property_name, "message") == 0) {
        *property_node = json_string_new(self->message);
    } else if (strcmp(property_name, "tags") == 0) {
        if (self->tags.elements) {
            json_node *tags_array = json_array_new();
            for (size_t i = 0; i < self->tags.length; i++)
                json_array_add_element(tags_array, json_integer_new(self->tags.elements[i]));
            *property_node = tags_array;
        } else {
            *property_node = NULL;
        }
    } else {
        json_serializable_fail_with_unhandled_property(property_name);
    }
    return json_serialization_status_continue;
}
