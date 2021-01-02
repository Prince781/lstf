#include "json/json-parser.h"
#include "json/json.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(void)
{
    json_node *parsed_node = json_parser_parse_string("{ \"pi\": 3.14159 }");

    if (!parsed_node) {
        fprintf(stderr, "failed to parse JSON node: %s\n", strerror(errno));
        return 1;
    }

    int retval = 0;
    json_node *object = json_object_new();
    json_object_set_member(object, "pi", json_double_new(3.14159));

    if (!json_node_equal_to(parsed_node, object))
        retval = 1;

    json_node_unref(parsed_node);
    json_node_unref(object);
    return retval;
}
