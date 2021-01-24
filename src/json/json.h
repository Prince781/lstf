#pragma once

#include "data-structures/ptr-hashmap.h"
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>

enum _json_node_type {
    json_node_type_null,
    json_node_type_integer,
    json_node_type_double,
    json_node_type_boolean,
    json_node_type_string,
    json_node_type_array,
    json_node_type_object,
    json_node_type_ellipsis             // non-standard JSON node, used for pattern matching
};
typedef enum _json_node_type json_node_type;

struct _json_node {
    json_node_type node_type;
    unsigned long refcount : sizeof(unsigned long)*CHAR_BIT - (1 + 1 + 1 + 1 + 1);

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

    /**
     * Whether this node should be partially matched to the other node, instead
     * of fully matched. Only meaningful for JSON objects.
     */
    bool partial_match : 1;
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

json_node *json_node_ref(json_node *node);

void json_node_unref(json_node *node);

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
 * Canonicalize to camelCase. Return value must be free()'d when done with.
 */
char *json_member_name_canonicalize(const char *member_name);

json_node *json_object_new(void);

json_node *json_object_pattern_new(void);

json_node *json_object_set_member(json_node *node, const char *member_name, json_node *member_value);

/**
 * Returns the member with `member_name`, or NULL if no such member exists.
 */
json_node *json_object_get_member(json_node *node, const char *member_name);

void json_object_pattern_set_is_partial_match(json_node *node);

/**
 * Creates a non-standard JSON ellipsis node, used for pattern matching in
 * `json_node_equal_to()`.  See `json_array_pattern_new()` and
 * `json_object_pattern_new()` for information about how this is used with
 * pattern JSON objects and pattern JSON arrays.
 */
json_node *json_ellipsis_new(void);
