#pragma once

#include "data-structures/collection.h"
#include "data-structures/ptr-hashmap.h"
#include "lstf-common.h"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdalign.h>

enum _json_node_type {
    json_node_type_null,
    json_node_type_integer,
    json_node_type_double,
    json_node_type_boolean,
    json_node_type_string,
    json_node_type_array,
    json_node_type_object,
    json_node_type_ellipsis,            // non-standard JSON node, used for pattern matching
    json_node_type_pointer              // non-standard JSON node, used for wrapping raw pointers
};
typedef enum _json_node_type json_node_type;

struct _json_node {
    alignas(8) json_node_type node_type;
    unsigned refcount : sizeof(unsigned)*CHAR_BIT - (1 + 1 + 1 + 1);

    /**
     * Whether this node is a floating reference.
     */
    bool floating : 1;

    /**
     * Used in recursive routines to handle cases where a JSON node circularly
     * refers to itself.
     */
    bool visiting : 1;

    /**
     * Whether this is a pattern JSON node.
     */
    bool is_pattern : 1;

    /**
     * Whether this node is optional in comparisons. This is useful, for
     * example, in optional properties:
     *
     * ```
     * {
     *  "optionalProperty"?: 3
     * }
     * ```
     */
    bool optional : 1;
};
typedef struct _json_node json_node;

struct _json_integer {
    json_node parent_struct;
    int64_t value;
};
typedef struct _json_integer json_integer;

struct _json_double {
    json_node parent_struct;
    double value;
};
typedef struct _json_double json_double;

struct _json_boolean {
    json_node parent_struct;
    bool value;
};
typedef struct _json_boolean json_boolean;

struct _json_string {
    json_node parent_struct;
    char *value;
};
typedef struct _json_string json_string;

struct _json_array {
    json_node parent_struct;
    json_node **elements;
    unsigned buffer_size;
    unsigned num_elements;
};
typedef struct _json_array json_array;

struct _ptr_list; // see ptr-list.h
typedef struct _ptr_list ptr_list;

struct _json_object {
    json_node parent_struct;
    ptr_hashmap *members;
};
typedef struct _json_object json_object;

struct _json_pointer {
    json_node parent_struct;
    void *value;
    collection_item_ref_func ref_func;
    collection_item_unref_func unref_func;
};
typedef struct _json_pointer json_pointer;

json_node *json_node_ref(json_node *node);

void json_node_unref(json_node *node);

static inline json_node *json_node_typecheck(json_node *node, json_node_type node_type)
{
    return node->node_type == node_type ? node : NULL;
}

#define json_node_cast(node, type)                                             \
    ((json_##type *)json_node_typecheck(node, json_node_type_##type))

#define json_array_foreach(array, element, statements) \
    for (unsigned i = 0; i < ((json_array *)array)->num_elements; ++i) { \
        json_node *element = ((json_array *)array)->elements[i]; \
        statements; \
    }

#define json_object_member_name(member) ((const char *)member->key)
#define json_object_member_node(member) ((json_node *)member->value)

#define json_object_foreach(object, member, statements) \
    for (iterator member##_it = ptr_hashmap_iterator_create(((json_object *)object)->members); \
            member##_it.has_next; member##_it = iterator_next(member##_it)) { \
        ptr_hashmap_entry *member = iterator_get_item(member##_it); \
        statements; \
    }

/**
 * Converts an unescaped JSON representation to an escaped form.
 */
char *json_string_escape(const char *unescaped);

/**
 * Returns a new string that is a representation of a JSON node. You must
 * free() this string afterwards.
 *
 * @param pretty    if `true`, then the string will be formatted and indented
 *                  across multiple lines
 */
char *json_node_to_string(json_node *node, bool pretty);

/**
 * Returns the size of what would be the stringified form of this JSON node.
 *
 * @see json_node_to_string
 */
size_t json_node_to_string_length(json_node *node, bool pretty);

/**
 * Compares two JSON nodes for strict equality.
 */
bool json_node_equal_to(json_node *node1, json_node *node2);

/**
 * Deep copies a JSON node.
 */
json_node *json_node_copy(json_node *node);

/**
 * Returns `node` with optional set to true
 */
static inline json_node *json_node_set_optional(json_node *node)
{
    node->optional = true;
    return node;
}

// --- creating new JSON nodes

json_node *json_null_new(void);

json_node *json_integer_new(int64_t value);

json_node *json_double_new(double value);

json_node *json_boolean_new(bool value);

json_node *json_string_new(const char *value);

json_node *json_array_new(void);

/**
 * Like `json_array_new()`, but allows this array to contain the non-standard
 * JSON ellipsis node, for pattern matching.
 */
json_node *json_array_pattern_new(void);

json_node *json_array_add_element(json_node *node, json_node *element);

/**
 * Sets the array element at `index` to `element`. Will raise an exception if
 * `index` is out of range.
 */
json_node *json_array_set_element(json_node *node, unsigned index, json_node *element);

/**
 * Returns the element at the index, or NULL if it does not exist (the index is out of range).
 */
json_node *json_array_get_element(json_node *node, unsigned index);

/**
 * Removes the element at position `index` and then shifts the elements in the
 * array from the right.
 */
void json_array_delete_element(json_node *node, unsigned index);

/**
 * Canonicalize to camelCase. Return value must be free()'d when done with.
 */
char *json_member_name_canonicalize(const char *member_name);

json_node *json_object_new(void);

json_node *json_object_pattern_new(void);

/**
 * Returns `member_value`
 */
json_node *json_object_set_member(json_node *node, const char *member_name, json_node *member_value);

/**
 * Returns the member with `member_name`, or NULL if no such member exists.
 */
json_node *json_object_get_member(json_node *node, const char *member_name);

void json_object_delete_member(json_node *node, const char *member_name);

/**
 * Creates a non-standard JSON ellipsis node, used for pattern matching in
 * `json_node_equal_to()`.  See `json_array_pattern_new()` and
 * `json_object_pattern_new()` for information about how this is used with
 * pattern JSON objects and pattern JSON arrays.
 */
json_node *json_ellipsis_new(void);

/**
 * Creates a non-standard JSON node wrapping a raw pointer.
 *
 * @param value         the raw pointer to wrap
 * @param ref_func      an operation to grab a reference to the object, or `NULL`
 * @param unref_func    an operation to release a reference to the object, or `NULL`
 */
json_node *json_pointer_new(void *value, collection_item_ref_func ref_func, collection_item_unref_func unref_func);
