#include "json-serializable.h"
#include "data-structures/collection.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashset.h"
#include "util.h"
#include "json/json.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>

json_serialization_status json_deserialize_from_node(const json_serializable_vtable *vtable,
                                                     void                           *instance,
                                                     json_node                      *node)
{
    assert((vtable->list_properties && vtable->deserialize_property) || !vtable->list_properties);

    json_serialization_status status = json_serialization_status_continue;

    if (vtable->list_properties) {
        // deserialize object
        if (node->node_type != json_node_type_object)
            return json_serialization_status_invalid_type;
        foreach (vtable->list_properties(), property_name, const char *, {
            json_node *property_node =
                json_object_get_member(node, property_name);
            bool is_optional = property_name[0] == '?';
            if (is_optional)
                property_name++;

            if (!property_node) {
                if (is_optional)
                    continue;
                return json_serialization_status_missing_property;
            }
            if ((status = vtable->deserialize_property(instance, property_name,
                                                       property_node)))
                return status;
        });
    } else if (vtable->list_elements) {
        // deserialize array
        json_array *jarr = json_node_cast(node, array);
        if (!jarr)
            return json_serialization_status_invalid_type;
        for (unsigned i = 0; i < jarr->num_elements; i++)
            if ((status = vtable->deserialize_element(instance, (void *)(intptr_t)i, jarr->elements[i])))
                return status;
    } else if (vtable->list_enum_values) {
        // deserialize an enum
        json_integer *jint = json_node_cast(node, integer);
        if (!jint)
            return json_serialization_status_invalid_type;
        // check for a valid value
        foreach (vtable->list_enum_values(), enum_val, intptr_t, {
            if (jint->value == (int64_t)enum_val) {
                *((int *)instance) = (int)enum_val;
                return json_serialization_status_continue;
            }
        });
        return json_serialization_status_invalid_type;
    } else {
        fprintf(stderr, "%s: unhandled type!\n", __func__);
        abort();
    }

    return status;
}

json_serialization_status json_serialize_to_node(const json_serializable_vtable *vtable,
                                                 const void                     *instance,
                                                 json_node                     **node)
{
    assert((vtable->list_properties && vtable->serialize_property) || !vtable->list_properties);

    json_serialization_status status = json_serialization_status_continue;

    if (vtable->list_properties) {
        // serialize object
        json_node *object = json_object_new();

        foreach (vtable->list_properties(), property_name, const char *, {
            json_node *property_node = NULL;
            bool       is_optional   = property_name[0] == '?';
            if (is_optional)
                property_name++;

            if ((status = vtable->serialize_property(instance, property_name,
                                                     &property_node))) {
                json_node_unref(object);
                return status;
            }

            if (property_node)
                json_object_set_member(object, property_name, property_node);
            else if (!is_optional) {
                json_node_unref(object);
                return json_serialization_status_missing_property;
            }
        });

        *node = object;
    } else if (vtable->list_elements) {
        // serialize array
        json_node *array = json_array_new();

        foreach (vtable->list_elements(instance), element, void *, {
            json_node *element_node = NULL;

            if ((status = vtable->serialize_element(instance, element,
                                                    &element_node))) {
                json_node_unref(array);
                return status;
            }

            assert(element_node &&
                   "serialize_element() did not initialize element node");
            json_array_add_element(array, element_node);
        });

        *node = array;
    } else if (vtable->list_enum_values) {
        // serialize enum
        const int *instance_int = instance;
        // check for a valid value
        foreach (vtable->list_enum_values(), enum_val, intptr_t, {
            if (*instance_int == (int)enum_val) {
                *node = json_integer_new(*instance_int);
                return json_serialization_status_continue;
            }
        });
        return json_serialization_status_invalid_type;
    } else {
        fprintf(stderr, "%s: unhandled type!\n", __func__);
        abort();
    }

    return status;
}
