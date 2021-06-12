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
        for (iterator it = vtable->list_properties(); it.has_next; it = iterator_next(it)) {
            const char *property_name = iterator_get_item(it);
            json_node *property_node = json_object_get_member(node, property_name);
            bool is_optional = property_name[0] == '?';
            if (is_optional)
                property_name++;

            if (!property_node) {
                if (is_optional)
                    continue;
                return json_serialization_status_missing_property;
            }
            if ((status = vtable->deserialize_property(instance, property_name, property_node)))
                return status;
        }
    } else {
        // deserialize array
        if (node->node_type != json_node_type_array)
            return json_serialization_status_invalid_type;
        for (unsigned i = 0; i < ((json_array *)node)->num_elements; i++)
            if ((status = vtable->deserialize_element(instance, ((json_array *)node)->elements[i])))
                return status;
    }

    return status;
}

json_serialization_status json_serialize_to_node(const json_serializable_vtable *vtable,
                                                 void                           *instance,
                                                 json_node                     **node)
{
    assert((vtable->list_properties && vtable->serialize_property) || !vtable->list_properties);

    json_serialization_status status = json_serialization_status_continue;

    if (vtable->list_properties) {
        // serialize object
        json_node *object = json_object_new();

        for (iterator it = vtable->list_properties(); it.has_next; it = iterator_next(it)) {
            const char *property_name = iterator_get_item(it);
            json_node *property_node = NULL;
            bool is_optional = property_name[0] == '?';
            if (is_optional)
                property_name++;

            if ((status = vtable->serialize_property(instance, property_name, &property_node))) {
                json_node_unref(object);
                return status;
            }

            if (property_node)
                json_object_set_member(object, property_name, property_node);
            else
                assert(is_optional && "serialize_property() did not initialize non-optional property node");
        }

        *node = object;
    } else {
        // serialize array
        json_node *array = json_array_new();

        for (iterator it = vtable->list_properties(); it.has_next; it = iterator_next(it)) {
            json_node *element_node = NULL;

            if ((status = vtable->serialize_element(instance, &element_node))) {
                json_node_unref(array);
                return status;
            }

            assert(element_node && "serialize_element() did not initialize element node");
            json_array_add_element(array, element_node);
        }

        *node = array;
    }

    return json_serialization_status_continue;
}
