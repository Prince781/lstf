#pragma once

#include "data-structures/closure.h"
#include "data-structures/ptr-hashmap.h"
#include "json/json-parser.h"
#include "json/json.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>

struct _jsonrpc_server;
typedef struct _jsonrpc_server jsonrpc_server;

typedef void (*jsonrpc_call_handler)(jsonrpc_server *server, const char *method, json_node *id, json_node *parameters, void *user_data);
typedef void (*jsonrpc_notification_handler)(jsonrpc_server *server, const char *method, json_node *parameters, void *user_data);

struct _jsonrpc_server {
    json_parser *parser;

    FILE *output_stream;
    bool close_output;

    /**
     * For handling JSON-RPC method calls.
     */
    ptr_hashmap *reply_handlers;

    /**
     * For handling JSON-RPC notifications. 
     */
    ptr_hashmap *notif_handlers;

    int64_t next_request_id;

    ptr_list *received_requests;
};

jsonrpc_server *jsonrpc_server_create(FILE *input_stream, bool close_input, 
                                      FILE *output_stream, bool close_output);

/**
 * Establish a new handler for a JSON-RPC call
 */
void jsonrpc_server_handle_call(jsonrpc_server          *server, 
                                const char              *method,
                                jsonrpc_call_handler     handler,
                                void                    *user_data,
                                closure_data_unref_func  user_data_unref_func);

/**
 * Establish a new handler for a JSON-RPC notification
 */
void jsonrpc_server_handle_notification(jsonrpc_server              *server, 
                                        const char                  *method,
                                        jsonrpc_notification_handler handler,
                                        void                        *user_data,
                                        closure_data_unref_func      user_data_unref_func);

/**
 * Synchronously replies to the remote. Should be called from inside a call
 * handler.
 */
void jsonrpc_server_reply_to_remote(jsonrpc_server  *server, 
                                    json_node       *id,
                                    json_node       *result);

/**
 * Call a method on the remote and wait for a response
 */
json_node *jsonrpc_server_call_remote(jsonrpc_server *server, const char *method, json_node *parameters);

/**
 * Send a notification to the remote.
 */
void jsonrpc_server_notify_remote(jsonrpc_server *server, const char *method, json_node *parameters);

/**
 * Wait for a specific notification to arrive from the remote.
 *
 * Returns a list of parameter objects for each time the [method] notification
 * was received, or NULL on error or timeout.
 */
ptr_list *jsonrpc_server_wait_for_notification(jsonrpc_server *server, const char *method);

/**
 * Waits synchronously for incoming messages (calls, notifications) and invokes
 * event handlers.  Returns -1 (EOF) if the underlying stream has been closed.
 * Returns -2 on error. Otherwise, the number of processed messages.
 */
int jsonrpc_server_process_incoming_messages(jsonrpc_server *server);

/**
 * Destroys the server, including any outstanding received requests.
 */
void jsonrpc_server_destroy(jsonrpc_server *server);
