#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include "jsonrpc/jsonrpc-server.h"
#include "json/json.h"
#include "io/outputstream.h"
#include "io/inputstream.h"
#include "io/event.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

static void
testmethod(jsonrpc_server *server, 
           const char *method, 
           json_node *id, 
           json_node *parameters, 
           void *user_data)
{
    int *const methods_invoked = user_data;

    if (!parameters)
        return;

    json_node *response = json_object_new();

    json_object_set_member(response, "method-name", json_string_new(method));
    json_object_set_member(response, "parameters", parameters);

    jsonrpc_server_reply_to_remote(server, id, response);

    (*methods_invoked)++;
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
        fprintf(stderr, "usage: %s file-to-parse\n", argv[0]); 
        return 1;
    }

    int methods_invoked = 0;
    jsonrpc_server *server = jsonrpc_server_new(
        json_file_to_parse, outputstream_new_from_file(stdout, false));
    eventloop *loop = eventloop_new();

    jsonrpc_server_handle_call(server, "test/methodA", testmethod,
                               &methods_invoked, NULL);
    jsonrpc_server_handle_call(server, "test/methodC", testmethod,
                               &methods_invoked, NULL);

    jsonrpc_server_listen(server, loop);
    while (eventloop_process(loop, false, NULL))
        ;

    eventloop_destroy(loop);
    jsonrpc_server_destroy(server);

    return methods_invoked == 2 ? 0 : 1;
}
