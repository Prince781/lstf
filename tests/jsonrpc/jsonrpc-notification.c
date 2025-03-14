#include "jsonrpc/jsonrpc-server.h"
#include "json/json.h"
#include "io/inputstream.h"
#include "io/event.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

static void
testmethod(jsonrpc_server *server, const char *method, json_node *parameters, void *user_data)
{
    (void) server;
    int *retval = user_data;

    json_node *comparison_array = json_array_new();
    json_array_add_element(comparison_array, json_integer_new(3));
    json_array_add_element(comparison_array, json_integer_new(4));

    if (strcmp(method, "testmethod") == 0 &&
            parameters && json_node_equal_to(comparison_array, parameters)) {
        *retval = 0;
        printf("received notification \"%s\"\n", method);
    }

    json_node_unref(comparison_array);
}

int main(int argc, char *argv[]) {
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

    int retval = 1;
    jsonrpc_server *server = jsonrpc_server_new(json_file_to_parse, outputstream_new_from_file(stdout, false));
    eventloop *loop = eventloop_new();

    jsonrpc_server_handle_notification(server, "testmethod", testmethod, &retval, NULL);
    jsonrpc_server_listen(server, loop);
    while (eventloop_process(loop, false, NULL))
        ;

    int error_code = server->error_code;
    eventloop_destroy(loop);
    jsonrpc_server_destroy(server);

    if (error_code) {
        fprintf(stderr, "JSON-RPC server has error condition set: %s\n",
                strerror(error_code));
        return 1;
    }

    return retval;
}
