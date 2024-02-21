#include "jsonrpc/jsonrpc-server.h"
#include "json/json.h"
#include "io/outputstream.h"
#include "io/inputstream.h"
#include "io/event.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

static void
testmethod(jsonrpc_server   *server, 
           const char       *method, 
           json_node        *id, 
           json_node        *parameters,
           void             *user_data)
{
    int *retval = user_data;
    if (!parameters)
        return;

    json_node *response = json_object_new();

    json_object_set_member(response, "method-name", json_string_new(method));
    json_object_set_member(response, "parameters", parameters);

    jsonrpc_server_reply_to_remote(server, id, response);

    *retval = 0;
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

    jsonrpc_server_handle_call(server, "testmethod", testmethod, &retval, NULL);
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
