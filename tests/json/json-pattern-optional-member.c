#include "json/json.h"
#include <stdbool.h>

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    bool matches = false;
    json_node *optional_obj = json_object_pattern_new();
    json_object_set_member(optional_obj, "version", json_node_set_optional(json_integer_new(3)));

    json_node *expression = json_object_new();
    // { version?: 3 } <=> {}
    matches = json_node_equal_to(optional_obj, expression)
        && json_node_equal_to(expression, optional_obj);

    json_node_unref(optional_obj);
    json_node_unref(expression);
    return !matches;
}
