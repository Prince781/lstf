#pragma once

#include "data-structures/ptr-hashmap.h"
#include "data-structures/string-builder.h"
#include "json/json.h"
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

enum _lstf_vm_value_type {
    /**
     * Indicates NULL as a special value
     */
    lstf_vm_value_type_null,

    /**
     * Indicates an integer value
     */
    lstf_vm_value_type_integer,

    /**
     * Indicates a double floating-point value
     */
    lstf_vm_value_type_double,

    /**
     * Indicates a boolean value
     */
    lstf_vm_value_type_boolean,

    /**
     * Stored as a `lstf_vm_string` object
     */
    lstf_vm_value_type_string,

    /**
     * A reference to a JSON object.
     */
    lstf_vm_value_type_object_ref,

    /**
     * A reference to a JSON array.
     */
    lstf_vm_value_type_array_ref,

    /**
     * A reference to a `lstf_vm_pattern` object.
     */
    lstf_vm_value_type_pattern_ref,

    /**
     * Address to code
     */
    lstf_vm_value_type_code_address
};
typedef enum _lstf_vm_value_type lstf_vm_value_type;

static_assert(sizeof(json_integer) - sizeof(json_node) >= sizeof(uint8_t *),
        "json_integer must be able to hold a code address");

struct _lstf_vm_value {
    lstf_vm_value_type value_type : 4;

    /**
     * Whether this value owns the object.
     */
    bool takes_ownership : 1;
    union {
        int64_t integer;

        double double_value;
        bool boolean;
        string *string;

        /**
         * Could be either a JSON object, JSON array, or a JSON pattern (object/array).
         */
        json_node *json_node_ref;

        /**
         * Could be either a code address to jump to or a function address to
         * call.
         */
        uint8_t *address;
    } data;
};
typedef struct _lstf_vm_value lstf_vm_value;

/**
 * Converts the value to a JSON node. Either creates a new JSON node or returns
 * an existing node that is referenced.
 */
static inline json_node *
lstf_vm_value_to_json_node(lstf_vm_value value)
{
    switch (value.value_type) {
    case lstf_vm_value_type_null:
        return json_null_new();
    case lstf_vm_value_type_boolean:
        return json_boolean_new(value.data.boolean);
    case lstf_vm_value_type_double:
        return json_double_new(value.data.double_value);
    case lstf_vm_value_type_integer:
        return json_integer_new(value.data.integer);
    case lstf_vm_value_type_string:
        return json_string_new(value.data.string->buffer);
    case lstf_vm_value_type_code_address:
        return json_integer_new((intptr_t) value.data.address);
    case lstf_vm_value_type_array_ref:
    case lstf_vm_value_type_object_ref:
    case lstf_vm_value_type_pattern_ref:
        return value.data.json_node_ref;
    }

    fprintf(stderr, "%s: unexpected value type %d\n", __func__, value.value_type); 
    abort();
}

static inline lstf_vm_value
lstf_vm_value_from_json_node(json_node *node)
{
    if (node->is_pattern &&
            (node->node_type == json_node_type_string || node->node_type == json_node_type_object))
        return (lstf_vm_value) {
            lstf_vm_value_type_pattern_ref,
            false,
            { .json_node_ref = node }
        };

    switch (node->node_type) {
    case json_node_type_array:
        return (lstf_vm_value) {
            lstf_vm_value_type_array_ref,
            false,
            { .json_node_ref = node }
        };
    case json_node_type_boolean:
        return (lstf_vm_value) {
            lstf_vm_value_type_boolean,
            true,
            { .boolean = ((json_boolean *)node)->value }
        };
    case json_node_type_double:
        return (lstf_vm_value) {
            lstf_vm_value_type_double,
            true,
            { .double_value = ((json_double *)node)->value }
        };
    case json_node_type_integer:
        return (lstf_vm_value) {
            lstf_vm_value_type_integer,
            true,
            { .integer = ((json_integer *)node)->value }
        };
    case json_node_type_null:
        return (lstf_vm_value) {
            lstf_vm_value_type_null,
            true,
            { .address = 0 }
        };
    case json_node_type_object:
        return (lstf_vm_value) {
            lstf_vm_value_type_object_ref,
            false,
            { .json_node_ref = node }
        };
    case json_node_type_string:
        return (lstf_vm_value) {
            lstf_vm_value_type_string,
            true,
            { .string = string_new_with_data(((json_string *)node)->value) } 
        };
    case json_node_type_ellipsis:
        fprintf(stderr, "%s: unreachable code: cannot convert JSON ellipsis to LSTF VM value\n", __func__);
        abort();
    }
}

/**
 * Performs cleanup on a value if it contains any data that must be
 * dereferenced.
 */
static inline void
lstf_vm_value_clear(lstf_vm_value *value)
{
    switch (value->value_type) {
    case lstf_vm_value_type_array_ref:
    case lstf_vm_value_type_object_ref:
    case lstf_vm_value_type_pattern_ref:
        if (value->takes_ownership)
            json_node_unref(value->data.json_node_ref);
        value->data.json_node_ref = NULL;
        break;
    case lstf_vm_value_type_boolean:
    case lstf_vm_value_type_code_address:
    case lstf_vm_value_type_double:
    case lstf_vm_value_type_integer:
    case lstf_vm_value_type_null:
        break;
    case lstf_vm_value_type_string:
        if (value->takes_ownership)
            string_unref(value->data.string);
        value->data.string = NULL;
        break;
    }

    value->takes_ownership = false;
}

static inline lstf_vm_value
lstf_vm_value_take_ownership(lstf_vm_value *value)
{
    lstf_vm_value new_value = {
        .value_type = value->value_type,
        .takes_ownership = true,
        .data = value->data
    };

    switch (new_value.value_type) {
    case lstf_vm_value_type_array_ref:
    case lstf_vm_value_type_object_ref:
    case lstf_vm_value_type_pattern_ref:
        json_node_ref(new_value.data.json_node_ref);
        break;
    case lstf_vm_value_type_string:
        string_ref(new_value.data.string);
        break;
    case lstf_vm_value_type_boolean:
    case lstf_vm_value_type_code_address:
    case lstf_vm_value_type_double:
    case lstf_vm_value_type_integer:
    case lstf_vm_value_type_null:
        break;
    }

    lstf_vm_value_clear(value);
    return new_value;
}
