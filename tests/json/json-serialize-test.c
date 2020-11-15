#include "data-structures/string-builder.h"
#include "json/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

bool pretty_print;

int main(int argc, char *argv[])
{
    FILE *json_file_to_compare_to = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--pretty-print") == 0)
            pretty_print = true;
        else if (!json_file_to_compare_to) {
            if (!(json_file_to_compare_to = fopen(argv[i], "r"))) {
                fprintf(stderr, "could not open %s - %s\n", argv[i], strerror(errno));
                return 1;
            }
        }
    }

    if (!json_file_to_compare_to) {
        fprintf(stderr, "usage: %s json-file-to-compare-to.json\n", argv[0]);
        return 1;
    }

    json_node *object = json_object_new();

    json_object_set_member(object, "null-property", json_null_new());
    json_object_set_member(object, "int-property", json_integer_new(INT64_MIN));
    json_object_set_member(object, "bool-property", json_boolean_new(true));
    json_object_set_member(object, "double-property", json_double_new(3.14159));
    json_object_set_member(object, "string-property", json_string_new("this is a string"));

    json_node *array_property = json_array_new();
    json_array_add_element(array_property, json_null_new());
    json_array_add_element(array_property, json_boolean_new(true));
    json_array_add_element(array_property, json_double_new(3.14159));
    json_array_add_element(array_property, json_string_new("this is another string, inside an array"));
    json_array_add_element(array_property, json_object_new());
    json_array_add_element(array_property, json_array_new());
    json_object_set_member(object, "array-property", array_property);

    json_node *object_property = json_object_new();
    json_object_set_member(object_property, "null-property", json_null_new());
    json_object_set_member(object_property, "string-property", json_string_new("Hello\u2014there"));
    json_object_set_member(object_property, "array-property", json_array_new());
    json_object_set_member(object, "object-property", object_property);

    char *serialized_string = json_node_to_string(object, pretty_print);
    printf("%s\n", serialized_string);
    int retval = 0;
    string *loaded_json = string_new();
    char buffer[1024] = { '\0' };
    int read_amt = 0;

    while ((read_amt = fread(buffer, 1, sizeof buffer - 1, json_file_to_compare_to)) > 0) {
        buffer[read_amt] = '\0';
        string_appendf(loaded_json, "%s", buffer); 
    }

    // remove trailing newline
    assert(loaded_json->length > 0);
    if (loaded_json->buffer[loaded_json->length - 1] == '\n') {
        loaded_json->length--;
        loaded_json->buffer[loaded_json->length] = '\0';
    }

    printf("--- compared to ---\n%s\n", loaded_json->buffer);

    retval = strcmp(serialized_string, loaded_json->buffer) != 0;

    json_node_unref(object);
    free(serialized_string);
    fclose(json_file_to_compare_to);
    free(string_destroy(loaded_json));
    return retval;
}
