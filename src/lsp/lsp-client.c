#include "lsp-client.h"
#include "data-structures/closure.h"
#include "io/event.h"
#include "io/io-common.h"
#include "jsonrpc/jsonrpc-server.h"
#include "version.h"
#include "json/json-serializable.h"
#include "json/json.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

json_serializable_impl_as_object(lsp_initializeparams, "rootPath", "processId", "clientInfo");

static json_serialization_status
lsp_initializeparams_deserialize_property(lsp_initializeparams *self,
                                          const char           *property_name,
                                          json_node            *property_node)
{
    (void) self;
    (void) property_name;
    (void) property_node;

    fprintf(stderr, "%s: deserialization not supported\n", __func__);
    abort();
}

static json_serialization_status
lsp_initializeparams_serialize_property(const lsp_initializeparams *self,
                                        const char                 *property_name,
                                        json_node                 **property_node)
{
    if (strcmp(property_name, "rootPath") == 0) {
        *property_node = json_string_new(self->root_path);
    } else if (strcmp(property_name, "processId") == 0) {
        *property_node = json_integer_new(self->process_id);
    } else if (strcmp(property_name, "clientInfo") == 0) {
        json_node *client_info_obj = json_object_new();
        json_object_set_member(client_info_obj, "name", json_string_new(self->client_info.name));
        json_object_set_member(client_info_obj, "version", json_string_new(self->client_info.version));
        *property_node = client_info_obj;
    } else {
        json_serializable_fail_with_unhandled_property(property_name);
    }

    return json_serialization_status_continue;
}

lsp_client *lsp_client_new(inputstream *istream, outputstream *ostream)
{
    lsp_client *client = calloc(1, sizeof *client);

    if (!client) {
        perror("failed to allocate new LSP server");
        abort();
    }

    jsonrpc_server_init(super(client), istream, ostream);
    array_init(&client->docs);

    return client;
}

void lsp_client_destroy(lsp_client *client)
{
    assert(client->docs.nofree);
    array_destroy(&client->docs);
    free(client->initialize_params.root_path);
    client->initialize_params.root_path = NULL;
    free(client->initialize_params.client_info.name);
    client->initialize_params.client_info.name = NULL;
    free(client->initialize_params.client_info.version);
    client->initialize_params.client_info.version = NULL;
    jsonrpc_server_destroy((jsonrpc_server *)client);
}

static void lsp_client_initialize_jsonrpc_call_remote_cb(const event *ev, void *user_data)
{
    event *initialize_server_ev = user_data;
    void *result = NULL;

    if (event_get_result(ev, &result)) {
        event_return(initialize_server_ev, result);
    } else {
        event_cancel_with_errno(initialize_server_ev, event_get_errno(ev));
    }
}

void lsp_client_initialize_server_async(lsp_client    *client,
                                        const char    *project_root,
                                        eventloop     *loop,
                                        async_callback callback,
                                        void          *callback_data)
{
    if (client->initialize_params.process_id != 0)
        return;     // already initialized

    client->initialize_params = (lsp_initializeparams) {
        .root_path = strdup(project_root),
        .process_id = io_getpid(),
        .client_info = {
            .name = strdup("Language Server Testing Framework"),
            .version = strdup(LSTF_VERSION)
        }
    };

    json_node *parameters = NULL;
    json_serialization_status status =
        json_serialize(lsp_initializeparams, &client->initialize_params, &parameters);
    assert(status == json_serialization_status_continue && "failed to serialize initialize parameters");

    event *initialize_server_ev = event_new(callback, callback_data);
    eventloop_add(loop, initialize_server_ev);

    jsonrpc_server_call_remote_async(super(client),
                                     "initialize",
                                     parameters,
                                     loop,
                                     lsp_client_initialize_jsonrpc_call_remote_cb,
                                     initialize_server_ev);
}

json_node *lsp_client_initialize_server_finish(const event *ev, int *error)
{
    void *result = NULL;

    if (error) *error = 0;

    if (!event_get_result(ev, &result)) {
        if (error)
            *error = event_get_errno(ev);
        return NULL;
    }

    return result;
}

void lsp_client_text_document_open_async(lsp_client       *client,
                                         lsp_textdocument *text_document,
                                         eventloop        *loop,
                                         async_callback    callback,
                                         void             *callback_data)
{
    assert(lsp_client_is_initialized(client) && "invoking textDocument/didOpen with uninitialized server!");

    json_node *parameters = NULL;
    json_serialization_status status =
        json_serialize(lsp_textdocument, text_document, &parameters);
    assert(status == json_serialization_status_continue && "failed to serialize text document");

    jsonrpc_server_call_remote_async(super(client),
                                     "textDocument/didOpen",
                                     parameters,
                                     loop,
                                     callback,
                                     callback_data);
}
