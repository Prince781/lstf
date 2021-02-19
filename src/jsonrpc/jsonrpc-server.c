#include "jsonrpc/jsonrpc-server.h"
#include "data-structures/closure.h"
#include "data-structures/collection.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/ptr-list.h"
#include "json/json-parser.h"
#include "data-structures/string-builder.h"
#include "util.h"
#include "json/json-scanner.h"
#include "json/json.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

enum _jsonrpc_error {
    jsonrpc_error_none = 0,
    jsonrpc_error_parse_error = -32700,
    jsonrpc_error_invalid_request = -32600,
    jsonrpc_error_method_not_found = -32601,
    jsonrpc_error_invalid_params = -32602,
    jsonrpc_error_internal_error = -32603
};
typedef enum _jsonrpc_error jsonrpc_error;

jsonrpc_server *jsonrpc_server_create(inputstream  *input_stream, 
                                      outputstream *output_stream)
{
    jsonrpc_server *server = calloc(1, sizeof *server);

    if (!server) {
        perror("could not create JSON-RPC server");
        abort();
    }

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
 * Returns number of bytes written, or < 0 if an error occurred.
 */
static int jsonrpc_server_send_message(jsonrpc_server *server, json_node *node)
{
    char *serialized_message = json_node_to_string(node, false);
    int ret = outputstream_printf(server->output_stream, "%s\n", serialized_message);
    free(serialized_message);
    return ret;
}

void jsonrpc_server_reply_to_remote(jsonrpc_server  *server, 
                                    json_node       *id,
                                    json_node       *result)
{
    json_node *response_object = json_object_new();

    json_object_set_member(response_object, "jsonrpc", json_string_new("2.0"));
    json_object_set_member(response_object, "result", result);
    json_object_set_member(response_object, "id", id);

    jsonrpc_server_send_message(server, response_object);
    json_node_unref(response_object);
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

json_node *jsonrpc_server_call_remote(jsonrpc_server *server,
                                      const char     *method,
                                      json_node      *parameters)
{
    json_node *request_object = json_object_new();
    json_node *request_id = NULL;
    json_node *response_node = NULL;

    json_object_set_member(request_object, "jsonrpc", json_string_new("2.0"));
    json_object_set_member(request_object, "method", json_string_new(method));
    json_object_set_member(request_object, "params", parameters);
    json_object_set_member(request_object, "id", request_id = json_integer_new(server->next_request_id));
    server->next_request_id++;

    if (jsonrpc_server_send_message(server, request_object) < 0) {
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

void jsonrpc_server_notify_remote(jsonrpc_server *server,
                                  const char     *method,
                                  json_node      *parameters)
{
    json_node *request_object = json_object_new();

    json_object_set_member(request_object, "jsonrpc", json_string_new("2.0"));
    json_object_set_member(request_object, "method", json_string_new(method));
    json_object_set_member(request_object, "params", parameters);

    if (jsonrpc_server_send_message(server, request_object) < 0)
        fprintf(stderr, "%s: output error: %s\n", __func__, strerror(errno));

    json_node_unref(request_object);
}

static bool jsonrpc_json_object_method_property_comparator(json_node *node1, json_node *node2)
{
    json_node *node1_method = json_object_get_member(node1, "method");
    json_node *node2_method = json_object_get_member(node2, "method");

    if (!node1_method || !node2_method)
        return false;

    return json_node_equal_to(node1_method, node2_method);
}

ptr_list *jsonrpc_server_wait_for_notification(jsonrpc_server *server, const char *method)
{
    ptr_list *received_params = NULL;

    if (server->received_requests->length > 0) {
        json_node *query_json = json_object_new();
        json_object_set_member(query_json, "method", json_string_new(method));

        ptr_list_node *query_result = ptr_list_find(server->received_requests, 
                                                    query_json, 
                                                    (collection_item_equality_func)jsonrpc_json_object_method_property_comparator);

        if (query_result) {
            json_node *result_parameters = json_object_get_member(ptr_list_node_get_data(query_result, json_node *), "params");
            if (!received_params)
                received_params = ptr_list_new((collection_item_ref_func) json_node_ref,
                                               (collection_item_unref_func) json_node_unref);
            if (result_parameters)
                ptr_list_append(received_params, result_parameters);
            else
                ptr_list_append(received_params, json_null_new());
            ptr_list_remove_link(server->received_requests, query_result);
        }

        json_node_unref(query_json);
    }

    if (!received_params) {
        // wait for incoming request
        // TODO: use timeout
        json_node *received_node = NULL;

        while ((received_node = json_parser_parse_node(server->parser)) ||
                server->parser->scanner->last_token != json_token_eof) {
            if (received_node) {
                const char *verification_failed_why = NULL;
                if (jsonrpc_verify_is_request_object(received_node, &verification_failed_why)) {
                    // this is a notification
                    if (!json_object_get_member(received_node, "id") &&
                            strcmp(((json_string *)json_object_get_member(received_node, "method"))->value, method) == 0)
                        break;
                    // otherwise, this is a method call or notification not for us
                    ptr_list_append(server->received_requests, received_node);
                } else if (received_node->node_type == json_node_type_array) {
                    // handle batched calls
                    for (unsigned i = 0; i < ((json_array *)received_node)->num_elements; i++) {
                        json_node *element = ((json_array *)received_node)->elements[i];
                        const char *error = NULL;
                        if (jsonrpc_verify_is_request_object(element, &error)) {
                            json_string *member_method = (json_string *) json_object_get_member(element, "method");

                            if (ptr_hashmap_get(server->reply_handlers, member_method->value) || 
                                    ptr_hashmap_get(server->notif_handlers, member_method->value)) {
                                ptr_list_append(server->received_requests, element);
                            } else if (strcmp(member_method->value, method) == 0) {
                                json_node *result_parameters = json_object_get_member(element, "params");
                                if (!received_params)
                                    received_params = ptr_list_new((collection_item_ref_func) json_node_ref,
                                                                   (collection_item_unref_func) json_node_unref);
                                if (result_parameters)
                                    ptr_list_append(received_params, result_parameters);
                                else
                                    ptr_list_append(received_params, json_null_new());
                            } else {
                                fprintf(stderr, "%s: warning: method \"%s\" not found\n",
                                        __func__, member_method->value);
                            }
                        } else {
                            fprintf(stderr, "%s: error: received unexpected non-request: %s\n", 
                                    __func__, error);
                        }
                    }
                    json_node_unref(received_node);
                } else {
                    fprintf(stderr, "%s: error: received unexpected non-request: %s\n", 
                            __func__, verification_failed_why);
                    json_node_unref(received_node);
                }
            } else {
                // received_node is NULL because of an error
                for (iterator it = json_parser_get_messages(server->parser); it.has_next; it = iterator_next(it))
                    fprintf(stderr, "%s\n", (char *) iterator_get_item(it));
            }
        }

        if (received_node) {
            json_node *result_parameters = json_object_get_member(received_node, "params");
            if (!received_params)
                received_params = ptr_list_new((collection_item_ref_func) json_node_ref,
                                               (collection_item_unref_func) json_node_unref);
            if (result_parameters)
                ptr_list_append(received_params, result_parameters);
            else
                ptr_list_append(received_params, json_null_new());
            json_node_unref(received_node);
        }
    }

    return received_params;
}

int jsonrpc_server_process_incoming_messages(jsonrpc_server *server)
{
    int num_processed = 0;

    if (server->received_requests->length == 0) {
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
            if (request_node->node_type == json_node_type_array) {
                for (unsigned i = 0; i < ((json_array *)request_node)->num_elements; i++) {
                    json_node *element = ((json_array *)request_node)->elements[i];

                    if (jsonrpc_verify_is_request_object(element, NULL)) {
                        json_string *member_method = (json_string *) json_object_get_member(element, "method");

                        if (ptr_hashmap_get(server->reply_handlers, member_method->value) || 
                                ptr_hashmap_get(server->notif_handlers, member_method->value)) {
                            have_valid_requests = true;
                            ptr_list_append(server->received_requests, element);
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

    // process all received requests first
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

    free(server);
}
