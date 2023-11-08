#pragma once

#include "data-structures/ptr-hashmap.h"
#include "data-structures/string-builder.h"
#include "json/json.h"
#include "io/outputstream.h"
#include <assert.h>
#include <stddef.h>
#include <limits.h>
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
    lstf_vm_value_type_code_address,

    /**
     * A function closure
     */
    lstf_vm_value_type_closure
};
typedef enum _lstf_vm_value_type lstf_vm_value_type;

static_assert(sizeof(json_integer) - sizeof(json_node) >= sizeof(uint8_t *),
        "json_integer must be able to hold a code address");

static inline bool lstf_vm_value_type_is_json(lstf_vm_value_type value_type)
{
    switch (value_type) {
        case lstf_vm_value_type_pattern_ref:
        case lstf_vm_value_type_object_ref:
        case lstf_vm_value_type_array_ref:
            return true;
        default:
            return false;
    }
}

typedef struct _lstf_vm_closure lstf_vm_closure;

struct _lstf_vm_value {
    lstf_vm_value_type value_type;

    /**
     * Whether this value owns the object.
     */
    bool takes_ownership;

    /**
     * Whether this value is captured by some closure, and therefore is aliased
     * by an up-value.
     *
     * This determines whether we copy this value to an up-value before it is
     * popped off the stack.
     */
    bool is_captured;

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

        lstf_vm_closure *closure;
    } data;
};
typedef struct _lstf_vm_value lstf_vm_value;

typedef struct _lstf_vm_program lstf_vm_program;
void lstf_vm_value_print(lstf_vm_value *value, lstf_vm_program *program,
                         outputstream *ostream);

typedef struct _lstf_vm_coroutine lstf_vm_coroutine;

struct _lstf_vm_upvalue {
    unsigned refcount : sizeof(unsigned) * CHAR_BIT - 1;
    bool floating : 1;

    /**
     * If the up-value points to a local variable living in the stack frame of
     * a function call.
     */
    bool is_local;

    union {
        struct {
            /**
             * Absolute position on the stack of the value, if `is_local` is true.
             */
            int64_t stack_offset;

            /**
             * (weak ref) The coroutine that this local value is valid in.
             */
            lstf_vm_coroutine *cr;
        };

        /**
         * The value copied when the stack value is uplifted (and `is_local` is
         * switched to false).
         */
        lstf_vm_value value;
    };
};
typedef struct _lstf_vm_upvalue lstf_vm_upvalue;

lstf_vm_upvalue *lstf_vm_upvalue_new(int64_t stack_offset, lstf_vm_coroutine *cr);

lstf_vm_upvalue *lstf_vm_upvalue_ref(lstf_vm_upvalue *upvalue);

void lstf_vm_upvalue_unref(lstf_vm_upvalue *upvalue);

struct _lstf_vm_closure {
    unsigned refcount : sizeof(unsigned) * CHAR_BIT - 1;
    bool floating : 1;
    uint8_t num_upvalues;
    uint8_t *code_address;
    lstf_vm_upvalue *upvalues[];
};
typedef struct _lstf_vm_closure lstf_vm_closure;

lstf_vm_closure *lstf_vm_closure_new(uint8_t         *code_address,
                                     uint8_t          num_upvalues,
                                     lstf_vm_upvalue *upvalues[]);

lstf_vm_closure *lstf_vm_closure_ref(lstf_vm_closure *closure);

void lstf_vm_closure_unref(lstf_vm_closure *closure);

static inline bool
lstf_vm_closure_equal_to(const lstf_vm_closure *closure1, const lstf_vm_closure *closure2)
{
    if (closure1->code_address != closure2->code_address)
        return false;

    if (closure1->num_upvalues != closure2->num_upvalues)
        return false;

    for (unsigned i = 0; i < closure1->num_upvalues; i++)
        if (closure1->upvalues[i] != closure2->upvalues[i])
            return false;

    return true;
}

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
    case lstf_vm_value_type_closure:
    {
        /**
         * a closure in JSON form should be
         * {
         *  "code-address": int64,
         *  "up-values": int64[]    // pointer
         * }
         */
        json_node *object = json_object_new();
        json_object_set_member(object, "code-address", json_integer_new((intptr_t) value.data.closure->code_address));
        json_node *up_values = json_array_new();
        json_object_set_member(object, "up-values", up_values);
        for (unsigned i = 0; i < value.data.closure->num_upvalues; i++)
            json_array_add_element(up_values, json_integer_new((intptr_t) value.data.closure->upvalues[i]));
        return object;
    }   break;
    }

    fprintf(stderr, "%s: unexpected value type `%u'\n", __func__, value.value_type); 
    abort();
}

static inline int64_t json_integer_destroy(json_node *node)
{
    assert(node->node_type == json_node_type_integer &&
            node->floating && node->refcount == 0);
    int64_t value = ((json_integer *)node)->value;
    json_node_unref(node);
    return value;
}

static inline double json_double_destroy(json_node *node)
{
    assert(node->node_type == json_node_type_double &&
            node->floating && node->refcount == 0);
    double value = ((json_double *)node)->value;
    json_node_unref(node);
    return value;
}

static inline bool json_boolean_destroy(json_node *node)
{
    assert(node->node_type == json_node_type_boolean &&
            node->floating && node->refcount == 0);
    bool value = ((json_boolean *)node)->value;
    json_node_unref(node);
    return value;
}

/**
 * Destroys a JSON string node, but not its data. The node must not be
 * referenced.
 */
static inline char *json_string_destroy(json_node *node)
{
    assert(node->node_type == json_node_type_string &&
            node->floating && node->refcount == 0);

    char *buffer = ((json_string *)node)->value;
    ((json_string *)node)->value = NULL;
    free(node);
    return buffer;
}

/**
 * Destroys a JSON pointer wrapper, but not its data. The node must not be
 * referenced.
 */
static inline void *json_pointer_destroy(json_node *node)
{
    assert(node->node_type == json_node_type_pointer &&
            node->floating && node->refcount == 0);

    void *pointer = ((json_pointer *)node)->value;
    ((json_pointer *)node)->value = NULL;
    free(node);
    return pointer;
}

static inline string *
string_new_from_json_string(json_node *node)
{
    assert(node->node_type == json_node_type_string);

    string *sb = NULL;

    if (node->floating) {
        sb = string_new_take_data(json_string_destroy(node));
    } else {
        sb = string_new_copy_data(((json_string *)node)->value);
    }

    return sb;
}

static inline lstf_vm_value
lstf_vm_value_from_json_node(json_node *node)
{
    switch (node->node_type) {
    case json_node_type_array:
        return (lstf_vm_value) {
            node->is_pattern ? lstf_vm_value_type_pattern_ref : lstf_vm_value_type_array_ref,
            node->floating,
            false,
            { .json_node_ref = node->floating ? json_node_ref(node) : node }
        };
    case json_node_type_boolean:
        return (lstf_vm_value) {
            lstf_vm_value_type_boolean,
            false,
            false,
            { .boolean = node->floating ? json_boolean_destroy(node) : ((json_boolean *)node)->value }
        };
    case json_node_type_double:
        return (lstf_vm_value) {
            lstf_vm_value_type_double,
            false,
            false,
            { .double_value = node->floating ? json_double_destroy(node) : ((json_double *)node)->value }
        };
    case json_node_type_integer:
        return (lstf_vm_value) {
            lstf_vm_value_type_integer,
            false,
            false,
            { .integer = node->floating ? json_integer_destroy(node) : ((json_integer *)node)->value }
        };
    case json_node_type_null:
        if (node->floating)
            json_node_unref(node);
        return (lstf_vm_value) {
            lstf_vm_value_type_null,
            false,
            false,
            { .address = 0 }
        };
    case json_node_type_object:
        return (lstf_vm_value) {
            node->is_pattern ? lstf_vm_value_type_pattern_ref : lstf_vm_value_type_object_ref,
            node->floating,
            false,
            { .json_node_ref = node->floating ? json_node_ref(node) : node }
        };
    case json_node_type_string:
        return (lstf_vm_value) {
            lstf_vm_value_type_string,
            true,
            false,
            { .string = string_new_from_json_string(node) }
        };
    case json_node_type_ellipsis:
        return (lstf_vm_value) {
            lstf_vm_value_type_pattern_ref,
            node->floating,
            false,
            { .json_node_ref = node->floating ? json_node_ref(node) : node }
        };
    case json_node_type_pointer:
        if (((json_pointer *)node)->ref_func == (collection_item_ref_func) lstf_vm_closure_ref) {
            return (lstf_vm_value) {
                lstf_vm_value_type_closure,
                node->floating,
                false,
                { .closure = node->floating ? json_pointer_destroy(node) : ((json_pointer *)node)->value }
            };
        }
        fprintf(stderr, "%s: unreachable code: cannot unwrap JSON wrapped pointer node\n", __func__);
        abort();
    }

    fprintf(stderr, "%s: invalid JSON node type `%u'\n", __func__, node->node_type);
    abort();
}

/**
 * Performs cleanup on a value if it contains any data that must be
 * dereferenced.
 */
static inline void
lstf_vm_value_clear(lstf_vm_value *value)
{
    assert(!value->is_captured && "cannot clear a VM value that is captured!");

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
    case lstf_vm_value_type_closure:
        if (value->takes_ownership)
            lstf_vm_closure_unref(value->data.closure);
        value->data.closure = NULL;
        break;
    }

    value->value_type = lstf_vm_value_type_null;
    value->takes_ownership = false;
}

static inline lstf_vm_value
lstf_vm_value_take_ownership(lstf_vm_value *value)
{
    lstf_vm_value new_value = {
        .value_type = value->value_type,
        .takes_ownership = true,
        .is_captured = false,
        .data = value->data
    };

    switch (new_value.value_type) {
    case lstf_vm_value_type_array_ref:
    case lstf_vm_value_type_object_ref:
    case lstf_vm_value_type_pattern_ref:
        json_node_ref(new_value.data.json_node_ref);
        // grabbed floating reference?
        if (new_value.data.json_node_ref->refcount == 1)
            value->takes_ownership = false;
        break;
    case lstf_vm_value_type_string:
        string_ref(new_value.data.string);
        // grabbed floating reference?
        if (new_value.data.string->refcount == 1)
            value->takes_ownership = false;
        break;
    case lstf_vm_value_type_closure:
        lstf_vm_closure_ref(new_value.data.closure);
        // grabbed floating reference?
        if (new_value.data.closure->refcount == 1)
            value->takes_ownership = false;
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
