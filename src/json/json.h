#pragma once

#include "data-structures/ptr-hashmap.h"
#include <stdbool.h>
#include <inttypes.h>

enum _json_node_type {
    json_node_type_null,
    json_node_type_integer,
    json_node_type_double,
    json_node_type_boolean,
    json_node_type_string,
    json_node_type_array,
    json_node_type_object
};
typedef enum _json_node_type json_node_type;

struct _json_node {
    json_node_type node_type;
    unsigned long refcount;
    bool floating;
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
    int buffer_size;
    int num_elements;
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

char *json_node_to_string(const json_node *node, bool pretty);

bool json_node_equal_to(const json_node *node1, const json_node *node2);

json_node *json_null_new(void);

json_node *json_integer_new(int64_t value);

json_node *json_double_new(double value);

json_node *json_boolean_new(bool value);

json_node *json_string_new(const char *value);

json_node *json_array_new(void);

json_node *json_array_add_element(json_node *node, json_node *element);

json_node *json_object_new(void);

json_node *json_object_set_member(json_node *node, const char *member_name, json_node *member_value);

json_node *json_object_get_member(const json_node *node, const char *member_name);
