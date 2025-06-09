#pragma once

#include "lstf-common.h"
#include "data-structures/iterator.h"
#include "json/json.h"
#include "util.h"       // for array_length()

enum _json_serialization_status {
    json_serialization_status_continue,
    json_serialization_status_missing_property,
    json_serialization_status_invalid_type
};
typedef enum _json_serialization_status json_serialization_status;

typedef struct {
    /**
     * Returns an iterator over all the property names of this type. Each call
     * to `iterator_get_item()` must return a string for a property name that
     * will be used when calling `deserialize_property()` /
     * `serialize_property()`. The string is owned by the implementation.
     *
     * This must be set if we are an object, and it must be NULL if we are an
     * array.
     */
    iterator (*list_properties)(void);

    /**
     * Returns an iterator over all of the enum members of this type.
     */
    iterator (*list_enum_values)(void);

    // --- deserialization
    union {
        /**
         * This handles deserialization of each property of the object. Should
         * only be set if we are an object.
         */
        json_serialization_status (*deserialize_property)(void       *self,
                                                          const char *property_name,
                                                          json_node  *property_node);
    };

    // --- serialization
    union {
        /**
         * This handles serialization of each property of the object. Should only
         * be set if we are an object.
         *
         * @param property_node     a reference to a pointer to a JSON node, which
         *                          this function should only set if successful
         */
        json_serialization_status (*serialize_property)(const void *self,
                                                        const char *property_name,
                                                        json_node **property_node);
    };
} json_serializable_vtable;

/**
 * Fully initializes data from a JSON node.
 *
 * @param vtable    the virtual table that knows how to handle `instance`
 * @param instance  the instantiated data that will be initialized from the JSON node
 * @param node      the JSON node to deserialize
 *
 * @return  `json_serialization_status_continue` on success, otherwise the specific error
 */
json_serialization_status json_deserialize_from_node(const json_serializable_vtable *vtable,
                                                     void                           *instance,
                                                     json_node                      *node)
    __attribute__((nonnull (1, 2, 3)));

/**
 * Fully serializes data to a new JSON node.
 *
 * @param vtable    the virtual table that knows how to handle `instance`
 * @param instance  the instantiated data that will be converted to JSON
 * @param node      a reference to a pointer to a JSON node, which will be set on success
 *
 * @return  `json_serialization_status_continue` on success and `node` is
 *          set, otherwise the specific error
 */
json_serialization_status json_serialize_to_node(const json_serializable_vtable *vtable,
                                                 const void                     *instance,
                                                 json_node                     **node)
    __attribute__((nonnull (1, 2, 3)));

/**
 * Deserializes `instance` from `node`, for any type.
 *
 * @see json_deserialize_from_node
 */
#define json_deserialize(type, instance, node)                                 \
    json_deserialize_from_node(                                                \
        &type##_json_serializable_vtable,                                      \
        _Generic((instance), type *: (instance), void *: (instance)), node)

/**
 * Serializes `instance` to `node`.
 *
 * @see json_serialize_to_node
 */
#define json_serialize(type, instance, node)                                   \
    json_serialize_to_node(&type##_json_serializable_vtable,                   \
                           _Generic((instance),                                \
                               const type *: (instance),                       \
                               type *: (instance),                             \
                               void *: (instance)),                            \
                           node)

/**
 * Declares the type as a JSON object. Put this in the C header.
 *
 * @see json_serializable_impl_as_object
 */
#define json_serializable_decl_as_object(type_name, struct_members)            \
    extern const json_serializable_vtable                                      \
                                  type_name##_json_serializable_vtable;        \
    typedef struct struct_members type_name

/**
 * Implements the type as a JSON object. Put this in the C source.
 *
 * Under the hood: implements `json_serializable_vtable.list_properties` and declares
 * `.deserialize_property` and `.serialize_property` methods that must be
 * defined.
 */
#define json_serializable_impl_as_object(type_name, ...)                       \
    static char *type_name##_properties_list[] = {__VA_ARGS__, NULL};          \
                                                                               \
    static iterator type_name##_list_properties_iterate(iterator it)           \
    {                                                                          \
        return (iterator){                                                     \
            .collection = type_name##_properties_list,                         \
            .iterate    = type_name##_list_properties_iterate,                 \
            .has_next   = it.counter + 1u <                                    \
                        array_length(type_name##_properties_list) - 1,         \
            .counter = it.counter + 1,                                         \
            .data    = (type_name##_properties_list)[it.counter + 1]};            \
    }                                                                          \
                                                                               \
    static iterator type_name##_list_properties(void)                          \
    {                                                                          \
        return (iterator){                                                     \
            .collection = type_name##_properties_list,                         \
            .is_first   = true,                                                \
            .iterate    = type_name##_list_properties_iterate,                 \
            .has_next   = array_length(type_name##_properties_list) - 1 > 0,   \
            .data       = (type_name##_properties_list)[0]};                   \
    }                                                                          \
                                                                               \
    /* serialization functions to implement */                                 \
    static json_serialization_status type_name##_deserialize_property(         \
        type_name *self, const char *property_name, json_node *property_node); \
                                                                               \
    static json_serialization_status type_name##_serialize_property(           \
        const type_name *self, const char *property_name,                      \
        json_node **property_node);                                            \
                                                                               \
    /* the virtual table */                                                    \
    const json_serializable_vtable type_name##_json_serializable_vtable = {    \
        .list_properties = type_name##_list_properties,                        \
        .deserialize_property =                                                \
            (json_serialization_status (*)(void *, const char *, json_node *)) \
                type_name##_deserialize_property,                              \
        .serialize_property = (json_serialization_status (*)(                  \
            const void *, const char *,                                        \
            json_node **))type_name##_serialize_property}

/**
 * Declares the enum type as a JSON value. Put this in the C header.
 */
#define json_serializable_decl_as_enum(type_name)                              \
    extern const json_serializable_vtable type_name##_json_serializable_vtable

/**
 * Implements the enum type as a JSON value. Put this in the C source.
 */
#define json_serializable_impl_as_enum(type_name, ...)                         \
    static type_name type_name##_enum_values[] = {__VA_ARGS__};                \
                                                                               \
    static iterator type_name##_list_enum_values_iterate(iterator it)          \
    {                                                                          \
        return (iterator){                                                     \
            .collection = type_name##_enum_values,                             \
            .iterate    = type_name##_list_enum_values_iterate,                \
            .has_next =                                                        \
                it.counter + 1u < array_length(type_name##_enum_values),       \
            .counter = it.counter + 1,                                         \
            .data =                                                            \
                (void *)(intptr_t)(type_name##_enum_values)[it.counter + 1]};  \
    }                                                                          \
                                                                               \
    static iterator type_name##_list_enum_values(void)                         \
    {                                                                          \
        return (iterator){                                                     \
            .collection = type_name##_enum_values,                             \
            .is_first   = true,                                                \
            .iterate    = type_name##_list_enum_values_iterate,                \
            .has_next   = array_length(type_name##_enum_values) > 0,           \
            .data       = (void *)(intptr_t)(type_name##_enum_values)[0]};     \
    }                                                                          \
                                                                               \
    /* the virtual table */                                                    \
    const json_serializable_vtable type_name##_json_serializable_vtable = {    \
        .list_enum_values = type_name##_list_enum_values}

#define json_serializable_fail_with_unhandled_property(property_name)          \
    do {                                                                       \
        fprintf(stderr, "%s: error: property `%s' unhandled!\n", __func__,     \
                property_name);                                                \
        abort();                                                               \
    } while (0)
