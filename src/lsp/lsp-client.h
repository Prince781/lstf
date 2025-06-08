#pragma once

#include "io/inputstream.h"
#include "jsonrpc/jsonrpc-server.h"
#include "io/event.h"
#include "data-structures/array.h"
#include "json/json-serializable.h"
#include "lsp-textdocument.h"
#include "lsp-diagnostic.h"
#include "lsp-window.h"
#include <stdbool.h>

/**
 * `interface InitializeParams`
 */
json_serializable_decl_as_object(lsp_initializeparams, {
    int process_id;

    struct {
        char *name;
        char *version;
    } client_info;

    char *root_path;

    // capabilities is serialized as an empty object

    // the rest of the properties are omitted
});

#define json_optional_member(type, name)                                       \
  type name : sizeof(type) * CHAR_BIT - 1;                                     \
  bool name##_enabled

#define json_optional_member_set(st, name, value)                              \
  (st)->name = value;                                                          \
  (st)->name##_enabled = true

/**
 * `interface PublishDiagnosticsParams`
 */
json_serializable_decl_as_object(lsp_publishdiagnosticsparams, {
    json_optional_member(unsigned, version);

    array(lsp_diagnostic) diagnostics;
});

static inline void
lsp_publishdiagnosticsparams_clear(lsp_publishdiagnosticsparams *params) 
{
    for (size_t i = 0; i < params->diagnostics.length; ++i)
        lsp_diagnostic_clear(&params->diagnostics.elements[i]);
    array_destroy(&params->diagnostics);
}

typedef struct {
    jsonrpc_server parent_struct;

    lsp_initializeparams initialize_params;

    array(lsp_textdocument) docs;

    // ptr_list<json_node *>
    ptr_list *diagnostics_results;

    /**
     * Waiters for `textDocument/publishDiagnostics` notification.
     * @see lsp_client_wait_for_diagnostics_async
     */
    array(event *) diagnostics_waiters;
} lsp_client;

/**
 * Creates a new JSON-RPC server listening on [istream] and sending messages
 * to [ostream] behaving as a client in the Language Server Protocol.
 *
 * @param loop the event loop to process requests and responses on
 */
lsp_client *lsp_client_new(eventloop    *loop,
                           inputstream  *istream,
                           outputstream *ostream,
                           io_process    process);

void lsp_client_destroy(lsp_client *server);

static inline bool lsp_client_is_initialized(const lsp_client *client)
{
    return client->initialize_params.process_id != 0;
}

/**
 * Sends the `initialize` request to the server and waits for a response.
 *
 * @param project_root      the root directory of the current project
 */
void lsp_client_initialize_server_async(lsp_client    *client,
                                        const char    *project_root,
                                        eventloop     *loop,
                                        async_callback callback,
                                        void          *callback_data);

/**
 * Gets the response from the server for the initialize request, or `NULL` if
 * there was an error.
 *
 * @see lsp_client_initialize_server_async()
 */
json_node *lsp_client_initialize_server_finish(const event *ev, int *error);

/**
 * Invokes the `textDocument/didOpen` notification on the server.
 */
void lsp_client_text_document_open_async(lsp_client             *client,
                                         lsp_textdocument const *text_document,
                                         eventloop              *loop,
                                         async_callback          callback,
                                         void                   *callback_data);

/**
 * Completes the `textDocument/didOpen` notification. If there was an error,
 * `*error` will contain the appropriate value.
 */
bool lsp_client_text_document_open_finish(event const *ev, int *error);

/**
 * Waits for another notification of `textDocument/diagnostics`. One incoming
 * notification may will trigger all waiters.
 */
void lsp_client_wait_for_diagnostics_async(lsp_client    *client,
                                           eventloop     *loop,
                                           async_callback callback,
                                           void          *callback_data);

/**
 * Gets the `PublishDiagnosticsParams` from the notification. Returns a new
 * object that must be destroyed.
 */
json_node *lsp_client_wait_for_diagnostics_finish(const event *ev, int *error);

/**
 * A typed handler for the `window/showMessage` notification.
 */
typedef void (*lsp_handler_window_show_message)(lsp_client                  *client,
                                                const lsp_showmessageparams *params,
                                                void                        *user_data);

/**
 * Registers an event handler for the `window/showMessage` notification from the
 * server.
 */
void lsp_client_on_window_show_message(lsp_client                     *client,
                                       lsp_handler_window_show_message handler,
                                       void                           *user_data,
                                       closure_data_unref_func         user_data_unref_func);
