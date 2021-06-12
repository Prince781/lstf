#pragma once

#include "data-structures/closure.h"
#include "data-structures/ptr-hashmap.h"
#include "io/inputstream.h"
#include "io/outputstream.h"
#include "io/event.h"
#include "json/json-parser.h"
#include "json/json.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

enum _jsonrpc_error {
    jsonrpc_error_none = 0,
    jsonrpc_error_parse_error = -32700,
    jsonrpc_error_invalid_request = -32600,
    jsonrpc_error_method_not_found = -32601,
    jsonrpc_error_invalid_params = -32602,
    jsonrpc_error_internal_error = -32603
};
typedef enum _jsonrpc_error jsonrpc_error;

struct _jsonrpc_server {
    json_parser *parser;

    outputstream *output_stream;

    /**
     * For handling JSON-RPC method calls.
     */
    ptr_hashmap *reply_handlers;

    /**
     * For handling JSON-RPC notifications. 
     */
    ptr_hashmap *notif_handlers;

    uint64_t next_request_id;

    ptr_list *received_requests;

    ptr_list *received_responses;
};
typedef struct _jsonrpc_server jsonrpc_server;

typedef void (*jsonrpc_call_handler)(jsonrpc_server *server,
                                     const char     *method,
                                     json_node      *id,
                                     json_node      *parameters,
                                     void           *user_data);

typedef void (*jsonrpc_notification_handler)(jsonrpc_server *server,
                                             const char     *method,
                                             json_node      *parameters,
                                             void           *user_data);

/**
 * Initializes a new JSON-RPC server listening for incoming requests on
 * [input_stream] and writing messages to [output_stream].
 */
void jsonrpc_server_init(jsonrpc_server *server,
                         inputstream    *input_stream, 
                         outputstream   *output_stream);


/**
 * Creates a new JSON-RPC server listening for incoming requests on
 * [input_stream] and writing messages to [output_stream].
 */
jsonrpc_server *jsonrpc_server_new(inputstream  *input_stream, 
                                   outputstream *output_stream);

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
 * Replies to the remote with success. Should be called from inside a call
 * handler.
 */
void jsonrpc_server_reply_to_remote(jsonrpc_server *server, 
                                    json_node      *id,
                                    json_node      *result);

/**
 * Replies to the remote with success, asynchronously. Should be called from
 * inside a call handler. Will invoke the callback when done.
 */
void jsonrpc_server_reply_to_remote_async(jsonrpc_server *server,
                                          json_node      *id,
                                          json_node      *result,
                                          eventloop      *loop,
                                          async_callback  callback,
                                          void           *user_data);

/**
 * Synchronously calls a method on the remote and waits for a response
 */
json_node *jsonrpc_server_call_remote(jsonrpc_server *server,
                                      const char     *method,
                                      json_node      *parameters);

/**
 * Asynchronously calls a method on the remote and `callback` is executed when
 * there is a response.
 *
 * Use `jsonrpc_server_call_remote_finish()` in the callback to get a `json_node
 * *` or an error code if getting the response failed.
 */
void jsonrpc_server_call_remote_async(jsonrpc_server *server,
                                      const char     *method,
                                      json_node      *parameters,
                                      eventloop      *loop,
                                      async_callback  callback,
                                      void           *user_data);

/**
 * Completes an asynchronous remote procedure call and returns the response.
 *
 * @see jsonrpc_server_call_remote_async
 */
json_node *jsonrpc_server_call_remote_finish(event *ev, int *error);

/**
 * Send a notification to the remote.
 */
void jsonrpc_server_notify_remote(jsonrpc_server *server,
                                  const char     *method,
                                  json_node      *parameters);

/**
 * Process received requests.
 *
 * @return the number of requests processed
 */
int jsonrpc_server_process_received_requests(jsonrpc_server *server);

/**
 * Waits synchronously for incoming messages (calls, notifications) and invokes
 * event handlers.  Returns -1 (EOF) if the underlying stream has been closed.
 * Returns -2 on error. Otherwise, returns the number of new incoming messages.
 *
 * @see jsonrpc_server_process_received_requests
 */
int jsonrpc_server_wait_for_incoming_messages(jsonrpc_server *server);

/**
 * Begin the asynchronous handling of incoming messages. Use `eventloop_loop()`
 * after calling this function.
 */
void jsonrpc_server_listen(jsonrpc_server *server, eventloop *loop);

/**
 * Destroys the server, including any outstanding received requests.
 */
void jsonrpc_server_destroy(jsonrpc_server *server);
