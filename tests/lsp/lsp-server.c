#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include "io/event.h"
#include "io/inputstream.h"
#include "io/outputstream.h"
#include "jsonrpc/jsonrpc-server.h"
#include "lsp/lsp-client.h"
#include "json/json-serializable.h"
#include "json/json.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- LSP types needed for this server

json_serializable_decl_as_object(lsp_servercapabilities, {
    lsp_textdocumentsynckind text_document_sync;
});

json_serializable_impl_as_object(lsp_servercapabilities, "?textDocumentSync");

static json_serialization_status lsp_servercapabilities_serialize_property(lsp_servercapabilities *self,
                                                                           const char             *property_name,
                                                                           json_node             **property_node)
{
    if (strcmp(property_name, "textDocumentSync") == 0) {
        *property_node = json_integer_new(self->text_document_sync);
    } else {
        json_serializable_fail_with_unhandled_property(property_name);
    }
    return json_serialization_status_continue;
}

static json_serialization_status lsp_servercapabilities_deserialize_property(lsp_servercapabilities *self,
                                                                             const char             *property_name,
                                                                             json_node              *property_node)
{
    if (strcmp(property_name, "textDocumentSync") == 0) {
        if (property_node->node_type != json_node_type_integer)
            return json_serialization_status_invalid_type;
        switch (((json_integer *)property_node)->value) {
            case lsp_textdocumentsynckind_none:
            case lsp_textdocumentsynckind_full:
            case lsp_textdocumentsynckind_incremental:
                self->text_document_sync = ((json_integer *)property_node)->value;
                break;
            default:
                return json_serialization_status_invalid_type;
        }
    } else {
        // ignore unknown property
    }
    return json_serialization_status_continue;
}

json_serializable_decl_as_object(lsp_serverinfo, {
    char *name;
    char *version;
});

json_serializable_impl_as_object(lsp_serverinfo, "name", "?version");

static json_serialization_status lsp_serverinfo_serialize_property(lsp_serverinfo *self,
                                                                   const char     *property_name,
                                                                   json_node     **property_node)
{
    if (strcmp(property_name, "name") == 0) {
        *property_node = json_string_new(self->name);
    } else if (strcmp(property_name, "version") == 0) {
        if (self->version)      // property is optional
            *property_node = json_string_new(self->version);
    } else {
        json_serializable_fail_with_unhandled_property(property_name);
    }
    return json_serialization_status_continue;
}

static json_serialization_status lsp_serverinfo_deserialize_property(lsp_serverinfo *self,
                                                                     const char     *property_name,
                                                                     json_node      *property_node)
{
    if (strcmp(property_name, "name") == 0) {
        if (property_node->node_type != json_node_type_string)
            return json_serialization_status_invalid_type;
        self->name = strdup(((json_string *)property_node)->value);
    } else if (strcmp(property_name, "version") == 0) {
        if (property_node->node_type != json_node_type_string)
            return json_serialization_status_invalid_type;
        self->version = strdup(((json_string *)property_node)->value);
    } else {
        // ignore unknown property
    }
    return json_serialization_status_continue;
}

/**
 * `interface InitializeResult`
 */
json_serializable_decl_as_object(lsp_initializeresult, {
    lsp_servercapabilities capabilities;

    lsp_serverinfo server_info;
});

json_serializable_impl_as_object(lsp_initializeresult, "capabilities", "?serverInfo");

static json_serialization_status lsp_initializeresult_serialize_property(lsp_initializeresult *self,
                                                                         const char           *property_name,
                                                                         json_node           **property_node)
{
    if(strcmp(property_name, "capabilities") == 0) {
        return json_serialize(lsp_servercapabilities, &self->capabilities, property_node);
    } else if (strcmp(property_name, "serverInfo") == 0) {
        return json_serialize(lsp_serverinfo, &self->server_info, property_node);
    } else {
        json_serializable_fail_with_unhandled_property(property_name);
    }
}

static json_serialization_status lsp_initializeresult_deserialize_property(lsp_initializeresult *self,
                                                                           const char           *property_name,
                                                                           json_node            *property_node)
{
    if (strcmp(property_name, "capabilities") == 0) {
        return json_deserialize(lsp_servercapabilities, &self->capabilities, property_node);
    } else if (strcmp(property_name, "serverInfo") == 0) {
        return json_deserialize(lsp_serverinfo, &self->server_info, property_node);
    } else {
        json_serializable_fail_with_unhandled_property(property_name);
    }
}

// --- the server itself

typedef struct {
    jsonrpc_server parent;
    lsp_initializeresult init_info;
} lsp_server;

static lsp_server *lsp_server_new(inputstream *istream, outputstream *ostream)
{
    lsp_server *server = calloc(1, sizeof *server);

    jsonrpc_server_init(&server->parent, istream, ostream);
    server->init_info.server_info.name = strdup("LSTF test server");
    return server;
}

static void lsp_server_destroy(lsp_server *server)
{
    free(server->init_info.server_info.name);
    jsonrpc_server_destroy((jsonrpc_server *)server);
}

static int return_code = 1;

static void lsp_server_initialize_cb(event *reply_ev, void *user_data)
{
    eventloop *loop = user_data;
    if (event_get_result(reply_ev, NULL)) {
        printf("replied to server successfully!\n");
        return_code = 0;
    } else {
        fprintf(stderr, "failed to reply to server - %s\n", strerror(event_get_errno(reply_ev)));
    }
    eventloop_quit(loop);
}

static void lsp_server_on_initialize(jsonrpc_server *server,
                                     const char     *method,
                                     json_node      *id,
                                     json_node      *parameters,
                                     void           *user_data)
{
    // ignore:
    (void) method;
    (void) parameters;
    eventloop *loop = user_data;

    lsp_server *self = (lsp_server *)server;
    json_node *result = NULL;

    // TODO: handle any kind of ID (integer, null, string) per the protocol
    printf("got initialize! [id=%"PRIi64"]\n", ((json_integer *)id)->value);
    if (json_serialize(lsp_initializeresult, &self->init_info, &result) == json_serialization_status_continue) {
        jsonrpc_server_reply_to_remote_async(server, id, result, loop, lsp_server_initialize_cb, loop);
    } else {
        fprintf(stderr, "failed to serialize lsp_initializeresult to JSON\n");
        eventloop_quit(loop);
    }
}

int main(int argc, char *argv[])
{
    inputstream *is;
    if (argc > 1) {
        if (strcmp(argv[1], "-") == 0)
            is = inputstream_new_from_file(stdin, false);
        else
            is = inputstream_new_from_path(argv[1], "r");
    } else {
        is = inputstream_new_from_file(stdin, false);
    }

    lsp_server *server = lsp_server_new(is, outputstream_new_from_file(stdout, false));
    eventloop *loop = eventloop_new();

    printf("server started. waiting for messages...\n");
    jsonrpc_server_handle_call(&server->parent, "initialize", lsp_server_on_initialize, loop, NULL);

    // begin asynchronous listening of incoming messages
    jsonrpc_server_listen(&server->parent, loop);

    // loop until we get an 'initialize' request
    while (eventloop_process(loop, false))
        jsonrpc_server_process_received_requests(&server->parent);

    // print scanner errors
    if (server->parent.parser->scanner->message)
        fprintf(stderr, "%s\n", server->parent.parser->scanner->message);
    // print parser errors
    for (iterator it = ptr_list_iterator_create(server->parent.parser->messages); it.has_next; it = iterator_next(it))
        fprintf(stderr, "%s\n", (char *)iterator_get_item(it));

    lsp_server_destroy(server);
    eventloop_destroy(loop);
    return return_code;
}
