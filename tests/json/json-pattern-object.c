#include "json/json.h"
#include <stdbool.h>

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    bool matches = false;
    json_node *pattern = json_object_pattern_new();
    json_node *expression = json_object_new();
    json_object_set_member(expression, "version", json_integer_new(3));

    // {} <=> { version: 3 }
    matches = json_node_equal_to(pattern, expression)
        && json_node_equal_to(expression, pattern);

    json_node_unref(pattern);
    json_node_unref(expression);
    return !matches;
}
