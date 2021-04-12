#include "json/json.h"
#include <stdbool.h>

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;
    bool matches = false;

    json_node *pattern = json_array_pattern_new();

    json_array_add_element(pattern, json_integer_new(1));
    json_array_add_element(pattern, json_integer_new(2));
    json_array_add_element(pattern, json_ellipsis_new());
    json_array_add_element(pattern, json_integer_new(10));

    json_node *expression = json_array_new();
    for (int i = 1; i <= 10; i++)
        json_array_add_element(expression, json_integer_new(i));

    // [1, 2, ..., 10] <=> [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    matches = json_node_equal_to(pattern, expression) &&
        json_node_equal_to(expression, pattern);

    json_node_unref(pattern);
    json_node_unref(expression);
    return !matches;
}
