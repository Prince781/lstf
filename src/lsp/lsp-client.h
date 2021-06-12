#pragma once

#include "io/inputstream.h"
#include "jsonrpc/jsonrpc-server.h"
#include "io/event.h"
#include "data-structures/array.h"
#include "json/json-serializable.h"
#include "lsp-textdocument.h"
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

struct _lsp_client {
    jsonrpc_server parent;

    lsp_initializeparams initialize_params;

    array(lsp_textdocument) docs;
};
typedef struct _lsp_client lsp_client;

/**
 * Creates a new JSON-RPC server listening on [istream] and sending messages
 * to [ostream] behaving as a client in the Language Server Protocol.
 */
lsp_client *lsp_client_new(inputstream *istream, outputstream *ostream);

void lsp_client_destroy(lsp_client *server);

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
json_node *lsp_client_initialize_server_finish(event *ev, int *error);
