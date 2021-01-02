#include "data-structures/iterator.h"
#include "json/json-parser.h"
#include "json/json.h"
#include "io/inputstream.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    inputstream *json_file_to_parse = NULL;
    for (int i = 1; i < argc; i++) {
        if (!json_file_to_parse) {
            if (!(json_file_to_parse = inputstream_new_from_path(argv[i], "r"))) {
                fprintf(stderr, "could not open %s - %s\n", argv[i], strerror(errno));
                return 1;
            }
        }
    }

    if (!json_file_to_parse) {
        fprintf(stderr, "usage: %s json-file-to-parse.json\n", argv[0]); 
        return 1;
    }

    int retval = 0;
    // create the JSON object
    json_node *object = json_object_new();

    json_object_set_member(object, "null-property", json_null_new());

    json_node *array_property = json_array_new();
    json_array_add_element(array_property, json_integer_new(1));
    json_array_add_element(array_property, json_integer_new(2));
    json_object_set_member(object, "array-property", array_property);

    json_node *object_property = json_object_new();
    json_object_set_member(object_property, "hello", json_string_new("goodbye"));
    json_object_set_member(object, "object-property", object_property);
    json_object_set_member(object, "int-property", json_integer_new(-340));

    // parse the JSON object from the file
    json_parser *parser = json_parser_create_from_stream(json_file_to_parse);
    json_node *parsed_node = NULL;

    if ((parsed_node = json_parser_parse_node(parser))) {
        retval = !json_node_equal_to(object, parsed_node);
    } else {
        fprintf(stderr, "parser: failed to parse JSON:\n"); 
        for (iterator it = json_parser_get_messages(parser);
                it.has_next;
                it = iterator_next(it))
            fprintf(stderr, "%s\n", (char *)iterator_get_item(it));
        retval = 1;
    }

    if (retval != 0) {
        char *serialized_pretty = json_node_to_string(object, true);
        fprintf(stdout, "%s\n:", serialized_pretty);
        free(serialized_pretty);
    }

    if (parsed_node)
        json_node_unref(parsed_node);
    json_node_unref(object);
    json_parser_destroy(parser);
    return retval;
}
