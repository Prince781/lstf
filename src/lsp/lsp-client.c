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
lsp_initializeparams_serialize_property(lsp_initializeparams *self,
                                        const char           *property_name,
                                        json_node           **property_node)
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

    jsonrpc_server_init(&client->parent, istream, ostream);
    array_init(&client->docs);

    return client;
}

void lsp_client_destroy(lsp_client *client)
{
    jsonrpc_server_destroy((jsonrpc_server *)client);
    assert(client->docs.nofree);
    array_destroy(&client->docs);
    free(client->initialize_params.root_path);
    free(client->initialize_params.client_info.name);
    free(client->initialize_params.client_info.version);
    memset(client, 0, sizeof *client);
    free(client);
}

struct initialize_server_ctx {
    lsp_client *client;
    event *server_reply_ev;
};

static void lsp_client_initialize_server_cb(event *ev, void *user_data)
{
    struct initialize_server_ctx *ctx = user_data;
    event *server_reply_ev = ctx->server_reply_ev;
    void *result = NULL;

    if (event_get_result(ev, &result)) {
        // propagate the result
        event_return(server_reply_ev, result);
    } else {
        event_cancel_with_errno(server_reply_ev, event_get_errno(ev));
    }
    free(ctx);
}

static void lsp_client_outputstream_ready_cb(event *ready_ev, void *user_data)
{
    struct initialize_server_ctx *ctx = user_data;
    lsp_client *client = ctx->client;
    event *server_reply_ev = ctx->server_reply_ev;

    if (!event_get_result(ready_ev, NULL)) {
        fprintf(stderr, "error: output stream cannot be written to");
        int errcode = event_get_errno(ready_ev);
        if (errcode != 0)
            fprintf(stderr, ": %s", strerror(errcode));
        fprintf(stderr, "\n");
        event_cancel_with_errno(server_reply_ev, errcode);
        free(ctx);
        return;
    }

    // the stream is ready for writing, so now we write some data and create an
    // event waiting for a response

    // serialize the initialize params
    json_node *node = NULL;
    json_serialization_status status = json_serialize(lsp_initializeparams, &ctx->client->initialize_params, &node);
    if (status != json_serialization_status_continue) {
        event_cancel_with_errno(server_reply_ev, errno);
        free(ctx);
    } else {
        // write an initialize request to the remote
        jsonrpc_server_call_remote_async(&client->parent, "initialize", node, ready_ev->loop, lsp_client_initialize_server_cb, ctx);
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

    jsonrpc_server *rpc_server = (jsonrpc_server *)client;

    client->initialize_params = (lsp_initializeparams) {
        .root_path = strdup(project_root),
        .process_id = io_getpid(),
        .client_info = {
            .name = strdup("Language Server Testing Framework"),
            .version = strdup(LSTF_VERSION)
        }
    };

    // wait for the outputstream to become ready for writing
    event *server_reply_ev = event_new(callback, callback_data);
    eventloop_add(loop, server_reply_ev);

    struct initialize_server_ctx *ctx = calloc(1, sizeof *ctx);
    *ctx = (struct initialize_server_ctx) {
        client,
        server_reply_ev
    };

    event *ostream_ready_ev = NULL;
    switch (rpc_server->output_stream->stream_type) {
    case outputstream_type_file:
        ostream_ready_ev =
            event_new_from_fd(outputstream_get_fd(rpc_server->output_stream),
                    false, lsp_client_outputstream_ready_cb, ctx);
        break;
    case outputstream_type_buffer:
        ostream_ready_ev = event_new(lsp_client_outputstream_ready_cb, ctx);
        break;
    }
    eventloop_add(loop, ostream_ready_ev);
}

json_node *lsp_client_initialize_server_finish(event *ev, int *error)
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
