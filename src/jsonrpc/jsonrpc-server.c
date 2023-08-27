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

void jsonrpc_server_init(jsonrpc_server *server,
                         inputstream    *input_stream, 
                         outputstream   *output_stream)
{
    server->parser = json_parser_create_from_stream(input_stream);
    server->output_stream = outputstream_ref(output_stream);

    server->reply_handlers = ptr_hashmap_new((collection_item_hash_func) strhash,
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

    server->received_responses = ptr_list_new((collection_item_ref_func) json_node_ref,
                                             (collection_item_unref_func) json_node_unref);
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
    ptr_hashmap_insert(server->reply_handlers, 
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
 * Returns success or failure.
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

static void jsonrpc_server_outputstream_ready_cb(const event *ready_ev,
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

static void jsonrpc_server_send_message_async(jsonrpc_server *server,
                                              json_node      *message,
                                              eventloop      *loop,
                                              async_callback  callback,
                                              void           *user_data)
{
    event *send_message_ev = event_new(callback, user_data);
    eventloop_add(loop, send_message_ev);

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
    event *ostream_ready_ev =
        event_new_from_fd(outputstream_get_fd(server->output_stream), false,
                jsonrpc_server_outputstream_ready_cb, ctx);
    eventloop_add(loop, ostream_ready_ev);
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

    event *replied_ev = event_new(callback, user_data);
    eventloop_add(loop, replied_ev);

    jsonrpc_server_send_message_async(server, response_object, loop, jsonrpc_server_reply_sent_cb, replied_ev);
}

static bool 
jsonrpc_verify_is_request_object(json_node      *node, 
                                 const char    **reason)
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
    if (strcmp(((json_string *)member_jsonrpc)->value, "2.0") != 0) {
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
jsonrpc_verify_is_response_object(json_node        *node,
                                  const char      **reason)
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
    if (strcmp(((json_string *)member_jsonrpc)->value, "2.0") != 0) {
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
        *request_id = json_object_set_member(request_object, "id", json_integer_new(server->next_request_id));
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

            if (ptr_hashmap_get(server->reply_handlers, member_method->value) ||
                    ptr_hashmap_get(server->notif_handlers, member_method->value)){
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
        for (iterator it = json_parser_get_messages(server->parser); it.has_next; it = iterator_next(it))
            fprintf(stderr, "%s: %s\n", __func__, (const char *) iterator_get_item(it));
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
        event *ready_ev = event_new_from_fd(fd, true, callback, user_data);
        eventloop_add(loop, ready_ev);
    }
}

struct parse_content_length_ctx {
    jsonrpc_server *server;
    event *header_parsed_ev;
    enum {
        parse_content_length_state_skipping_first,
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

    if (!event_get_result(ev, NULL)) {
        // error - cleanup
        goto cancel;
    }

    char read_character;
    if (!inputstream_read_char(ctx->server->parser->scanner->stream,
                               &read_character)) {
        // failed to read
        jsonrpc_debug(fprintf(stderr, "failed to read 'Content-Length: '\n"));
        goto cancel;
    } else {
        switch (ctx->state) {
        case parse_content_length_state_skipping_first:
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
                event_return(ctx->header_parsed_ev, (void *)content_length);
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
    event_cancel_with_errno(ctx->header_parsed_ev, EPROTO);
    free(ctx);
}

static size_t jsonrpc_server_parse_response_header_finish(const event *ev,
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

static void jsonrpc_server_parse_response_header_async(jsonrpc_server *server,
                                                       eventloop      *loop,
                                                       async_callback  callback,
                                                       void           *user_data)
{
    event *header_parsed_ev = event_new(callback, user_data);
    eventloop_add(loop, header_parsed_ev);

    struct parse_content_length_ctx *ctx;
    box(struct parse_content_length_ctx, ctx) {
        server,
        header_parsed_ev,
        .state = parse_content_length_state_skipping_first
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

struct parse_node_ctx {
    jsonrpc_server *server;
    json_node *response_id;
    event *response_handled_ev;
    eventloop *loop;
};

static void jsonrpc_server_parse_response_node_cb(const event *ev, void *user_data)
{
    struct parse_node_ctx *ctx = user_data;
    jsonrpc_server *server = ctx->server;
    json_node *response_id = ctx->response_id;
    event *response_handled_ev = ctx->response_handled_ev;
    void *result = NULL;

    if (event_get_result(ev, &result)) {
        json_node *received_node = result;

        jsonrpc_debug({
          char *id_str = json_node_to_string(response_id, false);
          char *received_str = json_node_to_string(received_node, true);
          fprintf(stderr, "[id: %s] received node:\n%s\n", id_str, received_str);
          free(id_str);
          free(received_str);
        });
        if (jsonrpc_verify_is_response_object(received_node, NULL)) {
            if (json_node_equal_to(json_object_get_member(received_node, "id"), response_id)) {
                event_return(response_handled_ev, received_node);
                json_node_unref(ctx->response_id);
                free(ctx);
                return;
            }

            // otherwise save the received node and listen for another response
            ptr_list_append(server->received_responses, received_node);
            json_parser_parse_node_async(server->parser, 
                    ctx->loop, jsonrpc_server_parse_response_node_cb, ctx);
        } else if (jsonrpc_verify_is_request_object(received_node, NULL)) {
            ptr_list_append(server->received_requests, received_node);
        } else {
            event_cancel_with_errno(response_handled_ev, EPROTO);
            json_node_unref(ctx->response_id);
            free(ctx);
        }
    } else {
        event_cancel_with_errno(response_handled_ev, event_get_errno(ev));
        json_node_unref(ctx->response_id);
        free(ctx);
    }
}

static bool jsonrpc_server_response_node_filter(void *item, void *user_data)
{
    json_node *node = item;
    json_node *response_id = user_data;

    return jsonrpc_verify_is_response_object(node, NULL)
        && json_node_equal_to(json_object_get_member(node, "id"), response_id);
}

struct parse_response_header_ctx {
    async_callback callback;
    void *callback_data;
    eventloop *loop;
    jsonrpc_server *server;
    json_node *response_id;
};

static void jsonrpc_server_parse_response_header_cb(const event *ev,
                                                    void        *user_data)
{
    struct parse_response_header_ctx *ctx = user_data;
    async_callback callback = ctx->callback;
    void *callback_data = ctx->callback_data;
    eventloop *loop = ctx->loop;
    jsonrpc_server *server = ctx->server;
    json_node *response_id = ctx->response_id;

    event *response_handled_ev = event_new(callback, callback_data);
    eventloop_add(loop, response_handled_ev);

    size_t content_length = 0;
    int error = 0;

    if ((content_length = jsonrpc_server_parse_response_header_finish(ev, &error))) {
        jsonrpc_debug({
          char *id_str = json_node_to_string(response_id, false);
          fprintf(stderr, "[id: %s] received header. "
                          "await json_parser_parse_node_async();\n"
                          "callback: => jsonrpc_server_parse_response_node_cb\n", id_str);
          free(id_str);
        });
        // schedule parsing the response node after success
        struct parse_node_ctx *ctx;
        box(struct parse_node_ctx, ctx) {
            server,
            json_node_ref(response_id),
            response_handled_ev,
            loop
        };
        json_parser_parse_node_async(server->parser, loop, jsonrpc_server_parse_response_node_cb, ctx);
    } else {
        // failure
        event_cancel_with_errno(response_handled_ev, error);
    }

    json_node_unref(response_id);
    free(ctx);
}

static void jsonrpc_server_handle_response_async(jsonrpc_server *server,
                                                 json_node      *response_id,
                                                 eventloop      *loop,
                                                 async_callback  callback,
                                                 void           *user_data)
{
    // first, check if we already have responses saved
    ptr_list_node *query = ptr_list_query(server->received_responses, jsonrpc_server_response_node_filter, response_id);
    if (query) {
        // we've found our response, so there's no need to run the event loop
        jsonrpc_debug({
          char *id_str = json_node_to_string(response_id, false);
          char *response_str = json_node_to_string(query->data, true);
          fprintf(stderr, "[id: %s]: already received response:\n%s\n", id_str,
                  response_str);
          free(id_str);
          free(response_str);
        });
        event tmp_ev = { 0 };
        event_return(&tmp_ev, query->data);
        callback(&tmp_ev, user_data);
        return;
    }

    struct parse_response_header_ctx *ctx;
    box(struct parse_response_header_ctx, ctx) {
        callback,
        user_data,
        loop,
        server,
        json_node_ref(response_id)
    };

    // parse the header asynchronously, then parse the node
    jsonrpc_debug({
      char *id_str = json_node_to_string(response_id, false);
      fprintf(stderr,
              "[id: %s]: await jsonrpc_server_parse_response_header_async();\n"
              "callback: => jsonrpc_server_parse_response_header_cb\n",
              id_str);
      free(id_str);
    });
    jsonrpc_server_parse_response_header_async(server, loop, jsonrpc_server_parse_response_header_cb, ctx);
}

static void jsonrpc_server_handle_response_cb(const event *receive_ev,
                                              void        *user_data)
{
    event *handle_ev = user_data;
    void *response = NULL;

    // just propagate the result of this receiving of the response to the event
    // for handling the response
    if (event_get_result(receive_ev, &response)) {
        event_return(handle_ev, response);
    } else {
        event_cancel_with_errno(handle_ev, event_get_errno(receive_ev));
    }
}

struct send_request_ctx {
    jsonrpc_server *server;
    json_node *request_id;
    event *response_ev;
};

static void jsonrpc_server_send_request_cb(const event *send_request_ev,
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
        // success - now wait for a response
        jsonrpc_debug({
          char *id_str = json_node_to_string(request_id, false);
          fprintf(stderr,
                  "[id: %s]: await jsonrpc_server_handle_response_async();\n"
                  "callback: => jsonrpc_server_handle_response_cb();\n",
                  id_str);
          free(id_str);
        });
        jsonrpc_server_handle_response_async(server, request_id, response_ev->loop,
                                             jsonrpc_server_handle_response_cb, response_ev);
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
    json_node *request_object = jsonrpc_server_create_request(server, method, parameters, &request_id);

    jsonrpc_debug({
      char *req_id_str = json_node_to_string(request_id, true);
      char *req_obj_str = json_node_to_string(request_object, true);
      fprintf(stderr, "[id: %s] call %s() with:\n%s\n",
              req_id_str, method, req_obj_str);
      free(req_id_str);
      free(req_obj_str);
    });

    event *response_ev = event_new(callback, user_data);
    eventloop_add(loop, response_ev);

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
                                      jsonrpc_server_send_request_cb, ctx);
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

int jsonrpc_server_process_received_requests(jsonrpc_server *server)
{
    int num_processed = 0;

    for (iterator it = ptr_list_iterator_create(server->received_requests);
            it.has_next;
            it = iterator_next(it)) {
        json_node *request_node = iterator_get_item(it);
        json_string *request_method = (json_string *) json_object_get_member(request_node, "method");
        json_node *request_id = json_object_get_member(request_node, "id");
        json_node *request_params = json_object_get_member(request_node, "params");

        if (request_id) {
            ptr_hashmap_entry *entry = ptr_hashmap_get(server->reply_handlers, request_method->value);
            closure *cl = entry->value;
            jsonrpc_call_handler handler = (jsonrpc_call_handler) cl->func_ptr;
            handler(server, request_method->value, request_id, request_params, cl->user_data);
        } else {
            ptr_hashmap_entry *entry = ptr_hashmap_get(server->notif_handlers, request_method->value);
            closure *cl = entry->value;
            jsonrpc_notification_handler handler = (jsonrpc_notification_handler) cl->func_ptr;
            handler(server, request_method->value, request_params, cl->user_data);
        }
        num_processed++;
    }
    ptr_list_clear(server->received_requests);

    return num_processed;
}

int jsonrpc_server_wait_for_incoming_messages(jsonrpc_server *server)
{
    unsigned num_received_requests = server->received_requests->length;

    if (num_received_requests == 0) {
        // wait for at least one message
        json_node *request_node = NULL;

        if (!(request_node = json_parser_parse_node(server->parser))) {
            // print parser errors
            for (iterator it = json_parser_get_messages(server->parser); it.has_next; it = iterator_next(it))
                fprintf(stderr, "%s: %s\n", __func__, (const char *) iterator_get_item(it));
            return server->parser->error ? -2 : -1;
        }

        if (!jsonrpc_verify_is_request_object(request_node, NULL)) {
            bool have_valid_requests = false;
            // handle batched requests
            if (request_node->node_type == json_node_type_array) {
                for (unsigned i = 0; i < ((json_array *)request_node)->num_elements; i++) {
                    json_node *element = ((json_array *)request_node)->elements[i];

                    if (jsonrpc_verify_is_request_object(element, NULL)) {
                        json_string *member_method = (json_string *) json_object_get_member(element, "method");

                        if (ptr_hashmap_get(server->reply_handlers, member_method->value) || 
                                ptr_hashmap_get(server->notif_handlers, member_method->value)) {
                            have_valid_requests = true;
                            ptr_list_append(server->received_requests, element);
                        } else {
                            fprintf(stderr, "%s: warning: method \"%s\" not found\n",
                                    __func__, member_method->value);
                        }
                    }
                }
            }
            json_node_unref(request_node);
            if (!have_valid_requests)
                return -2;
        } else {
            json_string *member_method = (json_string *) json_object_get_member(request_node, "method");

            if (ptr_hashmap_get(server->reply_handlers, member_method->value) || 
                    ptr_hashmap_get(server->notif_handlers, member_method->value)) {
                ptr_list_append(server->received_requests, request_node);
            } else {
                fprintf(stderr, "%s: warning: method \"%s\" not found\n",
                        __func__, member_method->value);
                json_node_unref(request_node);
            }
        }
    }

    return server->received_requests->length - num_received_requests;
}

static void jsonrpc_server_listen_parse_node_cb(const event *parse_node_ev, void *user_data)
{
    jsonrpc_debug();

    jsonrpc_server *server = user_data;
    int error = 0;
    json_node *node = json_parser_parse_node_finish(parse_node_ev, &error);

    if (node) {
        jsonrpc_debug({
          char *str = json_node_to_string(node, true);
          fprintf(stderr, "... => parsed node:\n%s\n", str);
          free(str);
        });
        // process the message
        if (jsonrpc_verify_is_request_object(node, NULL)) {
            json_string *member_method = (json_string *) json_object_get_member(node, "method");

            // save this message
            if (ptr_hashmap_get(server->reply_handlers, member_method->value) || 
                    ptr_hashmap_get(server->notif_handlers, member_method->value)) {
                ptr_list_append(server->received_requests, node);
            } else {
                // otherwise discard this node
                fprintf(stderr, "%s: warning: method \"%s\" not found\n", __func__, member_method->value);
                json_node_unref(node);
            }
        } else {
            // TODO: handle response
            json_node_unref(node);
        }

        // loop
        json_parser_parse_node_async(server->parser, parse_node_ev->loop, jsonrpc_server_listen_parse_node_cb, server);
    } else {
        // otherwise quit
        fprintf(stderr, "%s: warning: failed to parse node - %s\n", __func__, strerror(error));
    }
}

void jsonrpc_server_listen(jsonrpc_server *server, eventloop *loop)
{
    jsonrpc_debug(fprintf(stderr,
                          "await json_parser_parse_node_async();\n"
                          "callback: => jsonrpc_server_listen_parse_node_cb()\n"));
    json_parser_parse_node_async(server->parser, loop, jsonrpc_server_listen_parse_node_cb, server);
}

void jsonrpc_server_destroy(jsonrpc_server *server)
{
    json_parser_destroy(server->parser);
    server->parser = NULL;

    outputstream_unref(server->output_stream);
    server->output_stream = NULL;

    ptr_hashmap_destroy(server->reply_handlers);
    server->reply_handlers = NULL;
    ptr_hashmap_destroy(server->notif_handlers);
    server->notif_handlers = NULL;

    ptr_list_destroy(server->received_requests);
    server->received_requests = NULL;

    ptr_list_destroy(server->received_responses);
    server->received_responses = NULL;

    free(server);
}
