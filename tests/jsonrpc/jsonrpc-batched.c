#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include "jsonrpc/jsonrpc-server.h"
#include "json/json.h"
#include "io/outputstream.h"
#include "io/inputstream.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

static void
testMethod(jsonrpc_server *server, 
            const char *method, 
            json_node *id, 
            json_node *parameters, 
            void *user_data)
{
    int *methods_invoked = user_data;

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
    jsonrpc_server *server = jsonrpc_server_create(json_file_to_parse, outputstream_new_from_file(stdout, false));

    jsonrpc_server_handle_call(server, "test/methodA", testMethod, &methods_invoked, NULL);
    jsonrpc_server_handle_call(server, "test/methodC", testMethod, &methods_invoked, NULL);

    ptr_list *methodB_results = NULL;
    if ((methodB_results = jsonrpc_server_wait_for_notification(server, "test/methodB")))
        methods_invoked++;

    while (jsonrpc_server_process_incoming_messages(server) > 0)
        ;

    for (iterator it = ptr_list_iterator_create(methodB_results);
            it.has_next;
            it = iterator_next(it)) {
        json_node *methodB_parameters = iterator_get_item(it);
        json_node *status_obj = json_object_new();
        json_object_set_member(status_obj, "method", json_string_new("test/methodB"));
        json_object_set_member(status_obj, "method-b-parameters", methodB_parameters);
        jsonrpc_server_notify_remote(server, "postStatus", status_obj);
    }
    ptr_list_destroy(methodB_results);
    jsonrpc_server_destroy(server);

    return methods_invoked == 3 ? 0 : 1;
}
