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

json_serializable_impl_as_object(lsp_publishdiagnosticsparams, "?version", "diagnostics");

static json_serialization_status
lsp_publishdiagnosticsparams_deserialize_property(lsp_publishdiagnosticsparams *self,
                                                  const char                   *property_name,
                                                  json_node                    *property_node)
{
    if (strcmp(property_name, "version") == 0) {
        if (property_node->node_type != json_node_type_integer)
            return json_serialization_status_invalid_type;
        int64_t value = ((json_integer *)property_node)->value;
        json_optional_member_set(self, version, value);
    } else if (strcmp(property_name, "diagnostics") == 0) {
        if (property_node->node_type != json_node_type_array)
            return json_serialization_status_invalid_type;
        json_array_foreach(property_node, element, {
            lsp_diagnostic diag = {0};
            if (json_deserialize(lsp_diagnostic, &diag, element)) {
                lsp_diagnostic_clear(&diag);
                return json_serialization_status_invalid_type;
            }
            array_add(&self->diagnostics, diag);
        });
    } else {
        json_serializable_fail_with_unhandled_property(property_name);
    }

    return json_serialization_status_continue;
}

static json_serialization_status
lsp_publishdiagnosticsparams_serialize_property(const lsp_publishdiagnosticsparams *self,
                                                const char                         *property_name,
                                                json_node                         **property_node)
{
    (void) self;
    (void) property_name;
    (void) property_node;

    fprintf(stderr, "%s: serialization not supported\n", __func__);
    abort();
}

static void 
lsp_client_handle_publish_diagnostics(jsonrpc_server *server,
                                      const char     *method,
                                      json_node      *parameters,
                                      void           *user_data) 
{
    assert(!user_data && "unexpected user data");
    (void) method;

    lsp_client *client = (lsp_client *)server;

    if (client->diagnostics_waiters.length > 0) {
        for (size_t i = client->diagnostics_waiters.length; i > 0; --i) {
            event *ev = client->diagnostics_waiters.elements[i-1];
            event_return(ev, json_node_ref(parameters));
            array_remove(&client->diagnostics_waiters, i-1);
        }
    } else {
        // no waiters. save for later.
        ptr_list_append(client->diagnostics_results, parameters);
    }
}

lsp_client *lsp_client_new(inputstream  *istream, 
                           outputstream *ostream,
                           eventloop    *loop)
{
    lsp_client *client = calloc(1, sizeof *client);

    if (!client) {
        perror("failed to allocate new LSP server");
        abort();
    }

    jsonrpc_server_init(super(client), istream, ostream);
    array_init(&client->docs);
    client->diagnostics_results =
        ptr_list_new((collection_item_ref_func)json_node_ref,
                     (collection_item_unref_func)json_node_unref);
    array_init(&client->diagnostics_waiters);

    jsonrpc_server_handle_notification(super(client), 
            "textDocument/publishDiagnostics",
            lsp_client_handle_publish_diagnostics, NULL, NULL);

    jsonrpc_server_listen(super(client), loop);

    return client;
}

void lsp_client_destroy(lsp_client *client)
{
    free(client->initialize_params.root_path);
    client->initialize_params.root_path = NULL;
    free(client->initialize_params.client_info.name);
    client->initialize_params.client_info.name = NULL;
    free(client->initialize_params.client_info.version);
    client->initialize_params.client_info.version = NULL;

    assert(client->docs.nofree);
    array_destroy(&client->docs);

    ptr_list_destroy(client->diagnostics_results);

    assert(client->diagnostics_waiters.nofree);
    array_destroy(&client->diagnostics_waiters);

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
                                        void          *const callback_data)
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

    event *initialize_server_ev = eventloop_add(loop, callback, callback_data);

    jsonrpc_server_call_remote_async(super(client),
                                     "initialize",
                                     parameters,
                                     loop,
                                     lsp_client_initialize_jsonrpc_call_remote_cb,
                                     initialize_server_ev);
}

json_node *lsp_client_initialize_server_finish(const event *ev, int *const error)
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

static void
lsp_client_text_document_open_jsonrpc_server_notify_cb(const event *ev,
                                                       void        *user_data) 
{
    event *td_open_sent_ev = user_data;
    int errnum = 0;

    if (jsonrpc_server_notify_remote_finish(ev, &errnum)) {
        event_return(td_open_sent_ev, NULL);
    } else {
        event_cancel_with_errno(td_open_sent_ev, errnum);
    }
}

void lsp_client_text_document_open_async(lsp_client             *client,
                                         lsp_textdocument const *text_document,
                                         eventloop              *loop,
                                         async_callback          callback,
                                         void                   *callback_data)
{
    assert(lsp_client_is_initialized(client) &&
           "invoking textDocument/didOpen with uninitialized server!");

    json_node *td_json = NULL;
    json_serialization_status status =
        json_serialize(lsp_textdocument, text_document, &td_json);
    assert(status == json_serialization_status_continue &&
           "failed to serialize text document");

    json_node *parameters = json_object_new();
    json_object_set_member(parameters, "textDocument", td_json);

    event *textdocument_open_ev = eventloop_add(loop, callback, callback_data);
    jsonrpc_server_notify_remote_async(super(client),
                                       "textDocument/didOpen",
                                       parameters,
                                       loop,
                                       lsp_client_text_document_open_jsonrpc_server_notify_cb,
                                       textdocument_open_ev);
}

bool lsp_client_text_document_open_finish(event const *ev, int *error)
{
    if (event_get_result(ev, NULL))
        return true;
    if (error)
        *error = event_get_errno(ev);
    return false;
}

void lsp_client_wait_for_diagnostics_async(lsp_client    *client,
                                           eventloop     *loop,
                                           async_callback callback,
                                           void          *callback_data)
{
    event *diagnostics_ready_ev = eventloop_add(loop, callback, callback_data);
    array_add(&client->diagnostics_waiters, diagnostics_ready_ev);

    for (iterator it = ptr_list_iterator_create(client->diagnostics_results);
         it.has_next && client->diagnostics_waiters.length > 0;
         it = iterator_next(it)) {
        json_node *params_json = iterator_get_item(it); // FIXME: should we return ptr_list_node instead?
        // drain container of diagnostics
        for (unsigned j = client->diagnostics_waiters.length; j > 0; --j) {
            // return the parameter JSON
            event_return(client->diagnostics_waiters.elements[j-1], params_json);
            array_remove(&client->diagnostics_waiters, j-1);
        }
        ptr_list_node *list_node = it.data; // FIXME: this should be opaque!!!
        ptr_list_remove_link(client->diagnostics_results, list_node);
    }
}

json_node *lsp_client_wait_for_diagnostics_finish(const event *ev, int *error)
{
    void *result = NULL;

    if (!event_get_result(ev, &result)) {
        if (error)
            *error = event_get_errno(ev);
        return NULL;
    }

    return result;
}
