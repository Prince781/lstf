#include "data-structures/iterator.h"
#include "json/json-parser.h"
#include "json/json.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    FILE *json_file_to_parse = NULL;
    for (int i = 1; i < argc; i++) {
        if (!json_file_to_parse) {
            if (!(json_file_to_parse = fopen(argv[i], "r"))) {
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
    // create the JSON array
    json_node *array = json_array_new();
    json_array_add_element(array, json_double_new(1));
    json_array_add_element(array, json_double_new(1.4e-4));
    json_array_add_element(array, json_double_new(1.2e+10));
    json_array_add_element(array, json_double_new(3.14159265));

    // parse the JSON object from the file
    json_parser *parser = json_parser_create_from_stream(json_file_to_parse, true);
    json_node *parsed_node = NULL;

    if ((parsed_node = json_parser_parse_node(parser))) {
        retval = !json_node_equal_to(array, parsed_node);
    } else {
        fprintf(stderr, "parser: failed to parse JSON:\n"); 
        for (iterator it = json_parser_get_messages(parser);
                it.has_next;
                it = iterator_next(it))
            fprintf(stderr, "%s\n", (char *)iterator_get_item(it));
        retval = 1;
    }

    if (retval != 0) {
        char *serialized_pretty = json_node_to_string(array, true);
        char *serialized2 = parsed_node != NULL ? json_node_to_string(parsed_node, true) : NULL;
        printf("%s\n---compared to what was read:---\n%s", serialized_pretty, serialized2);
        free(serialized_pretty);
        free(serialized2);
    }

    if (parsed_node)
        json_node_unref(parsed_node);
    json_node_unref(array);
    json_parser_destroy(parser);
    return retval;
}
