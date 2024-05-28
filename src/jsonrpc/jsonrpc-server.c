#include "jsonrpc/jsonrpc-server.h"
#include "data-structures/closure.h"
#include "data-structures/collection.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include "json/json-parser.h"
#include "data-structures/string-builder.h"
#include "io/event.h"
#include "io/outputstream.h"
#include "util.h"
#include "json/json-scanner.h"
#include "json/json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifndef JSONRPC_DEBUG
#define jsonrpc_debug(stmts) 
#else
#define jsonrpc_debug(stmts)                                                   \
  do {                                                                         \
    const size_t len = sizeof(__func__) / sizeof(__func__[0]);                 \
    if (len >= 3 && __func__[len - 2] == 'b' && __func__[len - 3] == 'c')      \
      fprintf(stderr, "... => [JSONRPC CALLBACK %s:%d]\n", __func__,           \
              __LINE__);                                                       \
    else                                                                       \
      fprintf(stderr, "[JSONRPC %s:%d]\n", __func__, __LINE__);                \
    { stmts; }                                                                 \
    fprintf(stderr, "\n");                                                     \
  } while (0);
#endif

/**
 * Hash an ID for a response or request.
 */
static unsigned jsonrpc_id_hash(json_node *id) 
{
    switch (id->node_type) {
    case json_node_type_null:
        return 0;
    case json_node_type_integer:
        return (uint64_t)json_node_cast(id, integer)->value;
    case json_node_type_double: 
    {
        char *representation = json_node_to_string(id, false);
        unsigned hash = strhash(representation);
        free(representation);
        return hash;
    }
    case json_node_type_string:
        return strhash(json_node_cast(id, string)->value);
    default:
        fprintf(stderr,
                "%s: ID for request object must be a string, integer, double, "
                "or null\n",
                __func__);
        abort();
        break;
    }
}

void jsonrpc_server_init(jsonrpc_server *server,
                         inputstream    *input_stream, 
                         outputstream   *output_stream)
{
    server->parser = json_parser_create_from_stream(input_stream);
    server->output_stream = outputstream_ref(output_stream);

    server->call_handlers = ptr_hashmap_new((collection_item_hash_func) strhash,
            NULL,
            (collection_item_unref_func) free,
            (collection_item_equality_func) strequal,
            NULL,
            (collection_item_unref_func) closure_destroy);

    server->notif_handlers = ptr_hashmap_new((collection_item_hash_func) strhash,
            NULL,
            (collection_item_unref_func) free,
            (collection_item_equality_func) strequal,
            NULL,
            (collection_item_unref_func) closure_destroy);

    server->received_requests = ptr_list_new((collection_item_ref_func) json_node_ref,
                                             (collection_item_unref_func) json_node_unref);

    server->received_responses =
        ptr_hashmap_new((collection_item_hash_func)jsonrpc_id_hash,
                        (collection_item_ref_func)json_node_ref,
                        (collection_item_unref_func)json_node_unref,
                        (collection_item_equality_func)json_node_equal_to,
                        (collection_item_ref_func)json_node_ref,
                        (collection_item_unref_func)json_node_unref);

    server->response_events = ptr_hashmap_new(
        (collection_item_hash_func)jsonrpc_id_hash,
        (collection_item_ref_func)json_node_ref,
        (collection_item_unref_func)json_node_unref,
        (collection_item_equality_func)json_node_equal_to, NULL, NULL);
}

jsonrpc_server *jsonrpc_server_new(inputstream *input_stream,
                                   outputstream *output_stream)
{
    jsonrpc_server *server = calloc(1, sizeof *server);

    if (!server) {
        perror("could not create JSON-RPC server");
        abort();
    }

    jsonrpc_server_init(server, input_stream, output_stream);

    return server;
}

void jsonrpc_server_handle_call(jsonrpc_server          *server, 
                                const char              *method,
                                jsonrpc_call_handler     handler,
                                void                    *user_data,
                                closure_data_unref_func  user_data_unref_func)
{
    ptr_hashmap_insert(server->call_handlers, 
                        strdup(method), 
                        closure_new((closure_func) handler, user_data, user_data_unref_func));
}

void jsonrpc_server_handle_notification(jsonrpc_server              *server, 
                                        const char                  *method,
                                        jsonrpc_notification_handler handler,
                                        void                        *user_data,
                                        closure_data_unref_func      user_data_unref_func)
{
    ptr_hashmap_insert(server->notif_handlers, 
                        strdup(method), 
                        closure_new((closure_func) handler, user_data, user_data_unref_func));
}

/**
 * Sends a message synchronously. Returns success or failure.
 */
static bool jsonrpc_server_send_message(jsonrpc_server *server, json_node *node)
{
    bool success = false;
    char *serialized_message = json_node_to_string(node, false);
    char *content_length_header = string_destroy(
        string_newf("Content-Length: %zu\r\n", strlen(serialized_message)));

    // header field
    if (outputstream_write_string(server->output_stream, content_length_header) != strlen(content_length_header))
        goto cleanup;
    jsonrpc_debug(fprintf(stderr, "wrote: %s\n", content_length_header));

    // transition between header fields and actual content
    if (outputstream_write_string(server->output_stream, "\r\n") != 2)
        goto cleanup;
    jsonrpc_debug(fprintf(stderr, "wrote: \\r\\n\n"));

    // content
    if (outputstream_printf(server->output_stream, "%s\r\n", serialized_message) != strlen(serialized_message) + 2)
        goto cleanup;
    jsonrpc_debug(fprintf(stderr, "wrote: %s\\r\\n\n", serialized_message));

    success = true;

cleanup:
    free(serialized_message);
    free(content_length_header);
    return success;
}

struct ostream_ready_ctx {
    jsonrpc_server *server;
    json_node *message;
    event *send_message_ev;
};

static void
jsonrpc_server_send_message_outputstream_ready_cb(const event *ready_ev,
                                                  void        *user_data) 
{
    struct ostream_ready_ctx *ctx = user_data;

    if (!event_get_result(ready_ev, NULL)) {
        jsonrpc_debug(fprintf(stderr, "error, outputstream not ready\n"));
        event_cancel_with_errno(ctx->send_message_ev, errno);
    } else {
        // write to the outputstream and invoke the callback later when we're done
        // TODO: write partial output
        jsonrpc_debug(
            fprintf(stderr, "outputstream ready, will send message\n"));
        if (!jsonrpc_server_send_message(ctx->server, ctx->message)) {
            // an error occurred
            event_cancel_with_errno(ctx->send_message_ev, errno);
        } else {
            // success
            event_return(ctx->send_message_ev, NULL);
        }
    }

    json_node_unref(ctx->message);
    free(ctx);
}

/**
 * Sends a request (a call or notification) asynchronously.
 */
static void jsonrpc_server_send_message_async(jsonrpc_server *server,
                                              json_node      *message,
                                              eventloop      *loop,
                                              async_callback  callback,
                                              void           *user_data)
{
    event *send_message_ev = eventloop_add(loop, callback, user_data);

    struct ostream_ready_ctx *ctx = NULL;
    box(struct ostream_ready_ctx, ctx) {
        server,
        json_node_ref(message),
        send_message_ev
    };

    // TODO: handle outputstream backed by a buffer
    jsonrpc_debug({
      int outfd = outputstream_get_fd(server->output_stream);
      fprintf(stderr,
              "await can write to fd %d\n"
              "callback: => jsonrpc_server_outputstream_ready_cb();\n",
              outfd);
    });
    eventloop_add_fd(loop, outputstream_get_fd(server->output_stream), false,
                     jsonrpc_server_send_message_outputstream_ready_cb, ctx);
}

static bool jsonrpc_server_send_message_finish(const event *ev, int *error)
{
    if (event_get_result(ev, NULL))
        return true;
    if (error)
        *error = event_get_errno(ev);
    return false;
}

static json_node *jsonrpc_server_create_response(json_node *request_id,
                                                 json_node *result)
{
    json_node *response_object = json_object_new();

    json_object_set_member(response_object, "jsonrpc", json_string_new("2.0"));
    json_object_set_member(response_object, "result", result);
    json_object_set_member(response_object, "id", request_id);

    return response_object;
}

void jsonrpc_server_reply_to_remote(jsonrpc_server *server, 
                                    json_node      *id,
                                    json_node      *result)
{
    json_node *response_object = jsonrpc_server_create_response(id, result);

    jsonrpc_server_send_message(server, response_object);
    json_node_unref(response_object);
}

static void
jsonrpc_server_reply_sent_cb(const event *reply_sent_ev, void *user_data)
{
    event *replied_ev = user_data;
    int error = 0;

    if (!jsonrpc_server_send_message_finish(reply_sent_ev, &error))
        event_cancel_with_errno(replied_ev, error);
    else
        event_return(replied_ev, NULL);
}

void jsonrpc_server_reply_to_remote_async(jsonrpc_server *server,
                                          json_node      *id,
                                          json_node      *result,
                                          eventloop      *loop,
                                          async_callback  callback,
                                          void           *user_data)
{
    json_node *response_object = jsonrpc_server_create_response(id, result);

    event *replied_ev = eventloop_add(loop, callback, user_data);

    jsonrpc_server_send_message_async(server, response_object, loop,
                                      jsonrpc_server_reply_sent_cb, replied_ev);
}

static bool 
jsonrpc_verify_is_request_object(json_node         *node, 
                                 const char **const reason)
{
    if (reason)
        *reason = NULL;

    if (node->node_type != json_node_type_object) {
        if (reason)
            *reason = "node is not a JSON object";
        return false;
    }

    json_node *member_jsonrpc = NULL;
    if (!(member_jsonrpc = json_object_get_member(node, "jsonrpc"))) {
        if (reason)
            *reason = "node does not have \"jsonrpc\" property";
        return false;
    }
    if (member_jsonrpc->node_type != json_node_type_string) {
        if (reason)
            *reason = "\"jsonrpc\" property of node is not a JSON string";
        return false;
    }
    if (strcmp(json_node_cast(member_jsonrpc, string)->value, "2.0") != 0) {
        if (reason)
            *reason = "\"jsonrpc\" property of node is not equal to \"2.0\"";
        return false;
    }

    json_node *member_method = NULL;
    if (!(member_method = json_object_get_member(node, "method"))) {
        if (reason)
            *reason = "node does not have \"method\" property";
        return false;
    }
    if (member_method->node_type != json_node_type_string) {
        if (reason)
            *reason = "\"method\" property of node is not a JSON string";
        return false;
    }

    // ID if request (with expected response), or no ID if notification
    json_node *member_id = json_object_get_member(node, "id");
    if (member_id && !(member_id->node_type == json_node_type_string ||
                       member_id->node_type == json_node_type_integer ||
                       member_id->node_type == json_node_type_double ||
                       member_id->node_type == json_node_type_null)) {
        if (reason)
            *reason = "\"id\" property of node doesn't exist or is neither a JSON string, integer, nor null";
        return false;
    }

    // requests either have no params or if they do, they must be structured
    // (arrays or objects)
    json_node *member_params = json_object_get_member(node, "params");
    if (member_params && !(member_params->node_type == json_node_type_array ||
                           member_params->node_type == json_node_type_object)) {
        if (reason)
            *reason = "\"params\" must be array or object, or omitted";
        return false;
    }

    return true;
}

static bool
jsonrpc_verify_is_error_object(json_node *node)
{
    if (node->node_type != json_node_type_object)
        return false;

    json_node *member_code = NULL;
    if (!(member_code = json_object_get_member(node, "code")))
        return false;
    if (member_code->node_type != json_node_type_integer)
        return false;

    json_node *member_message = NULL;
    if (!(member_message = json_object_get_member(node, "message")))
        return false;
    if (member_message->node_type != json_node_type_string)
        return false;

    return true;
}

static bool
jsonrpc_verify_is_response_object(json_node         *node,
                                  const char **const reason)
{
    if (reason)
        *reason = NULL;

    if (node->node_type != json_node_type_object) {
        if (reason)
            *reason = "node is not an object";
        return false;
    }

    json_node *member_jsonrpc = NULL;
    if (!(member_jsonrpc = json_object_get_member(node, "jsonrpc"))) {
        if (reason)
            *reason = "node does not have \"jsonrpc\" property";
        return false;
    }
    if (member_jsonrpc->node_type != json_node_type_string) {
        if (reason)
            *reason = "\"jsonrpc\" property of node is not a JSON string";
        return false;
    }
    if (strcmp(json_node_cast(member_jsonrpc, string)->value, "2.0") != 0) {
        if (reason)
            *reason = "\"jsonrpc\" property of node is not equal to \"2.0\"";
        return false;
    }

    json_node *member_result = json_object_get_member(node, "result");
    json_node *member_error = json_object_get_member(node, "error");

    if (!member_result && !member_error) {
        if (reason)
            *reason = "neither \"result\" nor \"error\" fields exist";
        return false;
    }

    if (member_result && member_error) {
        if (reason)
            *reason = "both \"result\" and \"error\" fields exist";
        return false;
    }

    if (member_error && !jsonrpc_verify_is_error_object(member_error)) {
        if (reason)
            *reason = "\"error\" is not a valid JSON-RPC error object";
        return false;
    }

    json_node *member_id = json_object_get_member(node, "id");
    if (member_id && !(member_id->node_type == json_node_type_string ||
                member_id->node_type == json_node_type_integer ||
                member_id->node_type == json_node_type_null)) {
        if (reason)
            *reason = "\"id\" property of node doesn't exist or is neither a JSON string, integer, nor null";
        return false;
    }

    return true;
}

/**
 * Creates a call or a notification request.
 *
 * @param request_id        leave `NULL` if this is a notification
 */
static json_node *
jsonrpc_server_create_request(jsonrpc_server   *server,
                              const char       *method,
                              json_node        *parameters,
                              json_node **const request_id)
{
    json_node *request_object = json_object_new();

    json_object_set_member(request_object, "jsonrpc", json_string_new("2.0"));
    json_object_set_member(request_object, "method", json_string_new(method));
    json_object_set_member(request_object, "params", parameters);
    if (request_id) {
        *request_id = json_object_set_member(
            request_object, "id", json_integer_new(server->next_request_id));
        server->next_request_id++;
    }

    return request_object;
}

json_node *jsonrpc_server_call_remote(jsonrpc_server *server,
                                      const char     *method,
                                      json_node      *parameters)
{
    json_node *response_node = NULL;
    json_node *request_id = NULL;
    json_node *request_object = jsonrpc_server_create_request(server, method, parameters, &request_id);

    if (jsonrpc_server_send_message(server, request_object) == 0) {
        fprintf(stderr, "%s: output error: %s\n", __func__, strerror(errno));
        json_node_unref(request_object);
        return NULL;
    }

    // listen for response
    while ((response_node = json_parser_parse_node(server->parser))) {
        const char *verification_failed_why = NULL;
        if (jsonrpc_verify_is_response_object(response_node, &verification_failed_why)) {
            // check if this is the response for our request
            if (json_node_equal_to(json_object_get_member(response_node, "id"), request_id))
                break;
            // FIXME: handle this case better
            fprintf(stderr, "%s: error: single-threaded server received response for different call\n", __func__);
            abort();
        } else if (jsonrpc_verify_is_request_object(response_node, NULL)) {
            json_string *member_method = (json_string *) json_object_get_member(response_node, "method");

            if (ptr_hashmap_get(server->call_handlers, member_method->value) ||
                ptr_hashmap_get(server->notif_handlers, member_method->value)) {
                ptr_list_append(server->received_requests, response_node);
            } else {
                fprintf(stderr, "%s: warning: method \"%s\" not found\n",
                        __func__, member_method->value);
                json_node_unref(response_node);
            }
        } else {
            fprintf(stderr, "%s: warning: received invalid response object: %s\n", __func__, verification_failed_why);
            json_node_unref(response_node);
        }
    }

    if (!response_node) {
        // print parser errors if they occurred
        json_parser_messages_foreach(server->parser, message,
          fprintf(stderr, "%s: %s\n", __func__, message));
    }

    json_node_unref(request_object);

    return response_node;
}

static void jsonrpc_server_wait_stream_async(jsonrpc_server *server,
                                             eventloop      *loop,
                                             async_callback  callback,
                                             void           *user_data)
{
    int fd = inputstream_get_fd(server->parser->scanner->stream);
    if (fd == -1 || inputstream_ready(server->parser->scanner->stream)) {
        jsonrpc_debug(fprintf(stderr, "input stream is ready\n"));
        event tmp = { .loop = loop };
        event_return(&tmp, NULL);
        callback(&tmp, user_data);
    } else {
        // wait for the file to become readable
        jsonrpc_debug(fprintf(stderr, "wait for fd %d to become readable...\n", fd));
        eventloop_add_fd(loop, fd, true, callback, user_data);
    }
}

static bool jsonrpc_server_wait_stream_finish(const event *ev, int *error) {
    if (event_get_result(ev, NULL))
        return true;
    if (error)
        *error = event_get_errno(ev);
    return false;
}

struct parse_content_length_ctx {
    jsonrpc_server *server;
    event *header_parsed_ev;
    enum {
        parse_content_length_state_skipping_spaces,
        parse_content_length_state_skipping_text,
        parse_content_length_state_reading_length,
        parse_content_length_state_parse_newlines
    } state;
    unsigned i;
    char numbuf[20];
};

static void jsonrpc_server_parse_content_length_cb(const event *ev,
                                                   void        *user_data)
{
    struct parse_content_length_ctx *ctx = user_data;
    const char header_begin[] = "Content-Length: ";
    int errnum = 0;

    if (!jsonrpc_server_wait_stream_finish(ev, &errnum)) {
        // error - cleanup
        goto cancel;
    }

    char read_character;
    if (!inputstream_read_char(ctx->server->parser->scanner->stream,
                               &read_character)) {
        if ((errnum = errno) != 0) {
            // failed to read, not EOF
            jsonrpc_debug(fprintf(stderr,
                                  "failed to read 'Content-Length: ': %s\n",
                                  strerror(errno)));
        }
        goto cancel;
    } else {
        switch (ctx->state) {
        case parse_content_length_state_skipping_spaces:
            if (!isspace(read_character)) {
                ctx->state = parse_content_length_state_skipping_text;
                __attribute__((fallthrough));
            } else {
                break;
            }

        case parse_content_length_state_skipping_text:
            if (ctx->i < sizeof(header_begin) - 1) {
                if (read_character == header_begin[ctx->i])
                    // read another char
                    ++ctx->i;
                else
                    goto cancel;
            } else if (isdigit(read_character)) {
                // start of content length number
                ctx->state = parse_content_length_state_reading_length;
                ctx->i = 0;
                ctx->numbuf[ctx->i] = read_character;
                ++ctx->i;
            } else {
                // error
                goto cancel;
            }
            break;

        case parse_content_length_state_reading_length:
            if (isdigit(read_character)) {
                if (ctx->i == sizeof(ctx->numbuf) - 1) {
                    fprintf(stderr, "%s: too-long content length of %s%c!\n",
                            __func__, ctx->numbuf, read_character);
                    // throw "not supported" since JSON-RPC technically has no
                    // limit on content length
                    errnum = ENOTSUP;
                    goto cancel;
                } else {
                    // save char, read next
                    ctx->numbuf[ctx->i] = read_character;
                    ++ctx->i;
                }
            } else if (read_character == '\r') {
                // start of header end
                ctx->state = parse_content_length_state_parse_newlines;
            } else {
                // error
                goto cancel;
            }
            break;

        case parse_content_length_state_parse_newlines:
            if (read_character == '\n') {
                // success
                size_t content_length = 0;
                sscanf(ctx->numbuf, "%zu", &content_length);

                jsonrpc_debug(fprintf(
                    stderr, "done reading 'Content-Length: ', size = %zu\n",
                    content_length));
                event_return(ctx->header_parsed_ev,
                             (void *)(uintptr_t)content_length);
                free(ctx);
                return;
            } else {
                // error
                goto cancel;
            }
            break;
        }

        jsonrpc_debug(fprintf(
            stderr, "await jsonrpc_server_wait_stream_async();\n"
                    "callback: => jsonrpc_server_parse_content_length_cb\n"));
        jsonrpc_server_wait_stream_async(ctx->server, ev->loop,
                                         jsonrpc_server_parse_content_length_cb,
                                         user_data);
    }

    return;

cancel:
    event_cancel_with_errno(ctx->header_parsed_ev, errnum);
    free(ctx);
}

static size_t jsonrpc_server_parse_header_finish(const event *ev,
                                                 int         *error)
{
    void *result = NULL;

    if (!event_get_result(ev, &result)) {
        if (error)
            *error = event_get_errno(ev);
        return 0;
    }

    return (uintptr_t)result;
}

/**
 * (private API)
 *
 * Parses a header in the JSON-RPC stream. This is what comes before JSON-RPC
 * requests and responses.
 *
 * @param callback the callback to invoke after parsing the header
 */
static void jsonrpc_server_parse_header_async(jsonrpc_server *server,
                                              eventloop      *loop,
                                              async_callback  callback,
                                              void           *user_data)
{
    event *header_parsed_ev = eventloop_add(loop, callback, user_data);

    struct parse_content_length_ctx *ctx;
    box(struct parse_content_length_ctx, ctx) {
        server,
        header_parsed_ev,
        .state = parse_content_length_state_skipping_spaces
    };
    json_scanner_reset(server->parser->scanner);

    // 1. skip the response header beginning
    // 2. parse the content length number
    // 3. read that many bytes
    jsonrpc_debug(fprintf(
        stderr, "await jsonrpc_server_wait_stream_async();\n"
                "callback: => jsonrpc_server_parse_content_length_cb\n"));
    jsonrpc_server_wait_stream_async(server, loop, jsonrpc_server_parse_content_length_cb, ctx);
}

struct send_request_ctx {
    jsonrpc_server *server;
    json_node *request_id;
    event *response_ev;
};

static void jsonrpc_server_call_remote_send_message_cb(const event *send_request_ev,
                                                       void        *user_data)
{
    struct send_request_ctx *ctx = user_data;
    jsonrpc_server *server = ctx->server;
    json_node *request_id = ctx->request_id;
    event *response_ev = ctx->response_ev;

    if (!jsonrpc_server_send_message_finish(send_request_ev, NULL)) {
        // failed to send message
        jsonrpc_debug({
          char *id_str = json_node_to_string(request_id, false);
          fprintf(stderr,
                  "[id: %s]: failed to send request: %s\n",
                  id_str, strerror(event_get_errno(send_request_ev)));
          free(id_str);
        });
        event_cancel_with_errno(response_ev, event_get_errno(send_request_ev));
    } else {
        // success - now get the response
        jsonrpc_debug({
          char *id_str = json_node_to_string(request_id, false);
          fprintf(stderr,
                  "[id: %s]: await jsonrpc_server_handle_response_async();\n"
                  "callback: => jsonrpc_server_handle_response_cb();\n",
                  id_str);
          free(id_str);
        });

        ptr_hashmap_entry *query = ptr_hashmap_get(server->received_responses, request_id);
        if (query) {
            // we received a response already (XXX: how?)
            event_return(response_ev, json_node_ref(query->value));
            ptr_hashmap_delete(server->received_responses, query->key);
        } else {
            // no response yet exists. wait for one
            ptr_hashmap_insert(server->response_events, request_id, response_ev);
        }
    }

    json_node_unref(ctx->request_id);
    free(ctx);
}

void jsonrpc_server_call_remote_async(jsonrpc_server *server,
                                      const char     *method,
                                      json_node      *parameters,
                                      eventloop      *loop,
                                      async_callback  callback,
                                      void           *user_data)
{
    json_node *request_id = NULL;
    json_node *request_object =
        jsonrpc_server_create_request(server, method, parameters, &request_id);

    jsonrpc_debug({
      char *req_id_str = json_node_to_string(request_id, true);
      char *req_obj_str = json_node_to_string(request_object, true);
      fprintf(stderr, "[id: %s] call %s() with:\n%s\n",
              req_id_str, method, req_obj_str);
      free(req_id_str);
      free(req_obj_str);
    });

    event *response_ev = eventloop_add(loop, callback, user_data);

    struct send_request_ctx *ctx;
    box(struct send_request_ctx, ctx) {
        server,
        json_node_ref(request_id),
        response_ev
    };

    jsonrpc_debug({
      fprintf(
          stderr,
          "await jsonrpc_server_send_message_async();\n"
          "callback: => jsonrpc_server_send_request_cb();\n");
    });
    jsonrpc_server_send_message_async(server, request_object, loop,
                                      jsonrpc_server_call_remote_send_message_cb, ctx);
}

json_node *jsonrpc_server_call_remote_finish(const event *ev, int *error)
{
    void *result = NULL;

    if (!event_get_result(ev, &result)) {
        if (error)
            *error = event_get_errno(ev);
        return NULL;
    }

    return result;
}

void jsonrpc_server_notify_remote(jsonrpc_server *server,
                                  const char     *method,
                                  json_node      *parameters)
{
    json_node *request_object = jsonrpc_server_create_request(server, method, parameters, NULL);

    if (jsonrpc_server_send_message(server, request_object) == 0)
        fprintf(stderr, "%s: output error: %s\n", __func__, strerror(errno));

    json_node_unref(request_object);
}

static void jsonrpc_server_notify_remote_cb(const event *ev, void *user_data)
{
    event *notify_remote_ev = user_data;
    int error = 0;

    if (jsonrpc_server_send_message_finish(ev, &error)) {
        event_return(notify_remote_ev, NULL);
    } else {
        event_cancel_with_errno(notify_remote_ev, error);
    }
}

void jsonrpc_server_notify_remote_async(jsonrpc_server *server,
                                        const char     *method,
                                        json_node      *parameters,
                                        eventloop      *loop,
                                        async_callback  callback,
                                        void           *user_data)
{
    json_node *request_object =
        jsonrpc_server_create_request(server, method, parameters, NULL);
    event *notify_remove_ev = eventloop_add(loop, callback, user_data);
    jsonrpc_server_send_message_async(server, request_object, loop,
                                      jsonrpc_server_notify_remote_cb,
                                      notify_remove_ev);
}

bool jsonrpc_server_notify_remote_finish(const event *ev, int *error)
{
    if (event_get_result(ev, NULL))
        return true;
    if (error)
        *error = event_get_errno(ev);
    return false;
}

static void
jsonrpc_server_listen_parse_header_cb(const event *header_parsed_ev,
                                      void        *user_data);

static void jsonrpc_server_handle_request(jsonrpc_server *server,
                                          json_node      *parsed_node) 
{
    const char *method_name =
        json_node_cast(json_object_get_member(parsed_node, "method"), string)
            ->value;
    json_node *id = json_object_get_member(parsed_node, "id");
    json_node *params = json_object_get_member(parsed_node, "params");
    ptr_hashmap_entry *handler_entry = NULL;
    if (id &&
        (handler_entry = ptr_hashmap_get(server->call_handlers, method_name))) {
        closure *cl = handler_entry->value;
        jsonrpc_call_handler call_handler = (jsonrpc_call_handler)cl->func_ptr;
        call_handler(server, method_name, id, params, cl->user_data);
    } else if (!id && (handler_entry = ptr_hashmap_get(server->notif_handlers,
                                                       method_name))) {
        closure *cl = handler_entry->value;
        jsonrpc_notification_handler notif_handler =
            (jsonrpc_notification_handler)cl->func_ptr;
        notif_handler(server, method_name, params, cl->user_data);
    } else {
        jsonrpc_debug(
            fprintf(stderr, "warning: method \"%s\" not found\n", method_name));
    }
}

static void
jsonrpc_server_listen_parse_node_after_header_cb(const event *node_parsed_ev,
                                                 void        *user_data) 
{
    jsonrpc_server *server = user_data;
    eventloop *loop = node_parsed_ev->loop;
    json_node *parsed_node = NULL;
    int error = 0;

    if ((parsed_node = json_parser_parse_node_finish(node_parsed_ev, &error))) {
        // is this a request object, a response object, or a batch of requests?
        const char *reason = NULL;
        if (jsonrpc_verify_is_request_object(parsed_node, &reason)) {
            jsonrpc_server_handle_request(server, parsed_node);
        } else if (jsonrpc_verify_is_response_object(parsed_node, &reason)) {
            // first, check if there is an event waiting on completion of this method
            json_node *id = json_object_get_member(parsed_node, "id");
            json_node *result = json_object_get_member(parsed_node, "result");
            ptr_hashmap_entry *response_ev_entry = ptr_hashmap_get(server->response_events, id);
            if (response_ev_entry) {
                // complete the event by returning the parsed response
                event *response_ev = response_ev_entry->value;
                event_return(response_ev, json_node_ref(result));
            } else {
                // XXX: if there is no event, then we have a "response" without
                //      a corresponding request?
                jsonrpc_debug(fprintf(
                    stderr, "received response before request. saving...\n"));
                ptr_hashmap_insert(server->received_responses, id, parsed_node);
            }
        } else if (parsed_node->node_type == json_node_type_array) {
            // possible batched requests...
            json_array_foreach(parsed_node, parsed_element, {
              if (jsonrpc_verify_is_request_object(parsed_element, &reason)) {
                jsonrpc_server_handle_request(server, parsed_element);
              } else {
                // invalid - bail out
                char *representation =
                    json_node_to_string(parsed_element, true);
                fprintf(stderr,
                        "%s: expected request object in batch: %s\n%s\n",
                        __func__, reason, representation);
                free(representation);
                json_node_unref(parsed_node);
                return;
              }
            });
            json_node_unref(parsed_node);       // discard the array
            return;
        } else {
            // invalid JSON node - bail out
            char *representation = json_node_to_string(parsed_node, true);
            fprintf(stderr,
                    "%s: JSON-RPC: got something neither a request nor a "
                    "response: %s\n%s\n",
                    __func__, reason, representation);
            free(representation);
            json_node_unref(parsed_node);
            return;
        }

        // we want to continue parsing - loop
        jsonrpc_server_parse_header_async(
            server, loop, jsonrpc_server_listen_parse_header_cb, server);
    } else {
        // failed to parse node
        fprintf(stderr, "%s: JSON-RPC: failed to parse node: %s\n", __func__, strerror(error));
        json_parser_messages_foreach(server->parser, msg, {
            unsigned long index = index_of(msg);
            if (index == 0)
                fprintf(stderr, "parser/scanner error messages:\n");
            fprintf(stderr, " %2lu. %s\n", index + 1, msg);
        });
    }
}

/**
 * Loop endlessly, parsing nodes and headers.
 */
static void
jsonrpc_server_listen_parse_header_cb(const event *header_parsed_ev,
                                      void        *user_data)
{
    jsonrpc_server *server = user_data;
    size_t content_length;
    int error = 0;
    eventloop *loop = header_parsed_ev->loop;

    if ((content_length = jsonrpc_server_parse_header_finish(header_parsed_ev, &error))) {
        // schedule parsing the node after success
        json_parser_parse_node_async(server->parser, loop,
            jsonrpc_server_listen_parse_node_after_header_cb, server);
    } else {
        server->is_listening = false;
        // only report errors. no errors are EOF
        if ((server->error_code = error)) {
            fprintf(stderr, "%s: JSON-RPC server failed to listen: %s\n",
                    __func__, strerror(error));
        }
    }
}

void jsonrpc_server_listen(jsonrpc_server *server, eventloop *loop)
{
    assert(!jsonrpc_server_is_listening(server) && "JSON-RPC server already listening");
    jsonrpc_debug(fprintf(stderr,
                          "await jsonrpc_server_parse_response_header_async();\n"
                          "callback: => jsonrpc_server_listen_parse_response_header_cb()\n"));
    server->is_listening = true;
    jsonrpc_server_parse_header_async(server, loop,
                                      jsonrpc_server_listen_parse_header_cb, server);
}

void jsonrpc_server_destroy(jsonrpc_server *server)
{
    json_parser_destroy(server->parser);
    server->parser = NULL;

    outputstream_unref(server->output_stream);
    server->output_stream = NULL;

    ptr_hashmap_destroy(server->call_handlers);
    server->call_handlers = NULL;
    ptr_hashmap_destroy(server->notif_handlers);
    server->notif_handlers = NULL;

    ptr_list_destroy(server->received_requests);
    server->received_requests = NULL;

    ptr_hashmap_destroy(server->received_responses);
    server->received_responses = NULL;

    ptr_hashmap_destroy(server->response_events);
    server->response_events = NULL;

    free(server);
}
