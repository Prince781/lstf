#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include "data-structures/string-builder.h"
#include "io/event.h"
#include "json-parser.h"
#include "json-scanner.h"
#include "json/json.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

json_node *json_parser_parse_string(const char *str)
{
    json_parser *parser = json_parser_create_from_stream(inputstream_new_from_static_string(str));

    if (!parser)
        return NULL;

    json_node *node = json_parser_parse_node(parser);
    int saved_errno = errno;

    json_parser_destroy(parser);
    errno = saved_errno;
    return node;
}

json_parser *json_parser_create_from_stream(inputstream *stream)
{
    if (!stream)
        return NULL;

    json_parser *parser = calloc(1, sizeof *parser);

    if (!parser)
        return NULL;

    parser->scanner = json_scanner_create_from_stream(stream);
    parser->messages = ptr_list_new(NULL, free);

    return parser;
}

json_node *json_parser_parse_node(json_parser *parser)
{
    if (parser->error) {
        ptr_list_clear(parser->messages);
        parser->error = false;
    }

    json_token token = json_scanner_next(parser->scanner);

    switch (token) {
    case json_token_eof:
    case json_token_error:
        // scanner should have error
        return NULL;
    case json_token_colon:
    case json_token_comma:
    case json_token_closebrace:
    case json_token_closebracket:
    {
        string *sb = string_new();

        string_appendf(sb, "%s:%u:%u: error: unexpected %s (`%s') at start of parsing JSON node",
                parser->scanner->filename,
                parser->scanner->source_location.line, parser->scanner->source_location.column,
                json_token_to_string(token),
                parser->scanner->last_token_buffer);
        ptr_list_append(parser->messages, string_destroy(sb));
        parser->error = true;
        return NULL;
    }
    case json_token_keyword_null:
        return json_null_new();
    case json_token_keyword_true:
    case json_token_keyword_false:
        return json_boolean_new(token == json_token_keyword_true);
    case json_token_string:
        return json_string_new(parser->scanner->last_token_buffer);
    case json_token_integer:
    {
        int64_t value;
        sscanf(parser->scanner->last_token_buffer, "%"PRId64, &value);
        return json_integer_new(value);
    }
    case json_token_double:
        return json_double_new(atof(parser->scanner->last_token_buffer));
    case json_token_openbracket:
    {   // parse array
        json_node *array = json_array_new();
        json_node *element = NULL;
        json_sourceloc last_sourceloc = parser->scanner->source_location;

        while ((element = json_parser_parse_node(parser)) != NULL) {
            json_array_add_element(array, element);
            // expect either a ',' or ']'
            token = json_scanner_next(parser->scanner);
            if (!(token == json_token_comma || token == json_token_closebracket)) {
                string *sb = string_new();
                string_appendf(sb, "%s:%u:%u: error: expected comma or close bracket", 
                        parser->scanner->filename, 
                        parser->scanner->source_location.line, parser->scanner->source_location.column);
                ptr_list_append(parser->messages, string_destroy(sb));
                parser->error = true;
                json_node_unref(array);
                return NULL;
            }
            last_sourceloc = parser->scanner->source_location;
            if (token == json_token_closebracket)
                break;
        }

        if (parser->scanner->last_token != json_token_closebracket) {
            string *sb = string_new();
            string_appendf(sb, "%s:%u:%u: error: expected close bracket at end of array",
                    parser->scanner->filename,
                    parser->scanner->source_location.line, parser->scanner->source_location.column);
            ptr_list_append(parser->messages, string_destroy(sb));
            parser->error = true;
            json_node_unref(array);
            return NULL;
        }

        if (((json_array *)array)->num_elements > 0 &&
                token != json_token_closebracket) {
            string *sb = string_new();
            string_appendf(sb, "%s:%u:%u: error: trailing %s",
                    parser->scanner->filename,
                    last_sourceloc.line, last_sourceloc.column,
                    json_token_to_string(token));
            ptr_list_append(parser->messages, string_destroy(sb));
            parser->error = true;
            json_node_unref(array);
            return NULL;
        }

        return array;
    }
    case json_token_openbrace:
    {   // parse object
        json_node *object = json_object_new();
        json_sourceloc last_sourceloc = parser->scanner->source_location;

        while (json_scanner_next(parser->scanner) == json_token_string) {
            char *member_name = strdup(parser->scanner->last_token_buffer);

            if (json_scanner_next(parser->scanner) != json_token_colon) {
                string *sb = string_new();
                string_appendf(sb, "%s:%u:%u: error: expected colon",
                        parser->scanner->filename,
                        parser->scanner->source_location.line, parser->scanner->source_location.column);
                ptr_list_append(parser->messages, string_destroy(sb));
                parser->error = true;
                json_node_unref(object);
                free(member_name);
                return NULL;
            }

            json_node *member_value = json_parser_parse_node(parser);
            if (!member_value) {
                string *sb = string_new();
                string_appendf(sb, "%s:%u:%u: error: expected JSON value for property `%s'",
                        parser->scanner->filename,
                        parser->scanner->source_location.line, parser->scanner->source_location.column,
                        member_name);
                ptr_list_append(parser->messages, string_destroy(sb));
                parser->error = true;
                json_node_unref(object);
                free(member_name);
                return NULL;
            }

            json_object_set_member(object, member_name, member_value);

            // expect either a ',' or '}'
            token = json_scanner_next(parser->scanner);
            if (!(token == json_token_comma || token == json_token_closebrace)) {
                string *sb = string_new();
                string_appendf(sb, "%s:%u:%u: error: expected comma or close brace", 
                        parser->scanner->filename, 
                        parser->scanner->last_token_begin.line, parser->scanner->last_token_begin.column);
                ptr_list_append(parser->messages, string_destroy(sb));
                parser->error = true;
                json_node_unref(object);
                free(member_name);
                return NULL;
            }
            last_sourceloc = parser->scanner->source_location;

            free(member_name);
            if (token == json_token_closebrace)
                break;
        }

        if (parser->scanner->last_token != json_token_closebrace) {
            string *sb = string_new();
            string_appendf(sb, "%s:%u:%u: error: expected close brace at end of object",
                    parser->scanner->filename,
                    parser->scanner->source_location.line, parser->scanner->source_location.column);
            ptr_list_append(parser->messages, string_destroy(sb));
            parser->error = true;
            json_node_unref(object);
            return NULL;
        }

        if (!ptr_hashmap_is_empty(((json_object *)object)->members) &&
                token != json_token_closebrace) {
            string *sb = string_new();
            string_appendf(sb, "%s:%u:%u: error: trailing %s",
                    parser->scanner->filename,
                    last_sourceloc.line, last_sourceloc.column,
                    json_token_to_string(token));
            ptr_list_append(parser->messages, string_destroy(sb));
            parser->error = true;
            json_node_unref(object);
            return NULL;
        }

        return object;
    }
    }

    fprintf(stderr, "invalid JSON token `%u'", token);
    abort();
}

struct parse_array_ctx {
    json_parser *parser;
    json_node *array;
    event *node_parsed_ev;
};

static void json_parser_parse_array_element_cb(const event *ev, void *user_data);

static void json_parse_array_element_scanner_next_cb(const event *ev, void *user_data)
{
    int errnum = 0;
    json_token token = json_scanner_next_finish(ev, &errnum);
    struct parse_array_ctx *ctx = user_data;
    json_parser *parser = ctx->parser;
    json_node *array = ctx->array;
    event *node_parsed_ev = ctx->node_parsed_ev;

    if (token == json_token_closebracket) {
        // we finished parsing the array
        event_return(node_parsed_ev, array);
        free(ctx);
    } else if (token == json_token_comma) {
        // continue parsing the next element
        json_parser_parse_node_async(parser, node_parsed_ev->loop, json_parser_parse_array_element_cb, ctx);
    } else {
        string *sb = string_new();
        string_appendf(sb, "%s:%u:%u: error: expected comma or close bracket", 
                parser->scanner->filename, 
                parser->scanner->source_location.line, parser->scanner->source_location.column);
        ptr_list_append(parser->messages, string_destroy(sb));
        parser->error = true;
        json_node_unref(array);
        event_cancel_with_errno(node_parsed_ev, errnum);
        free(ctx);
    }
}

static void json_parser_parse_array_element_cb(const event *ev, void *user_data)
{
    int errnum = 0;
    json_node *element = json_parser_parse_node_finish(ev, &errnum);
    struct parse_array_ctx *ctx = user_data;
    json_parser *parser = ctx->parser;
    json_node *array = ctx->array;
    event *node_parsed_ev = ctx->node_parsed_ev;

    if (!element) {     // bail out of parsing loop
        if (parser->scanner->last_token != json_token_closebracket) {
            string *sb = string_new();
            string_appendf(sb, "%s:%u:%u: error: expected close bracket at end of array",
                    parser->scanner->filename,
                    parser->scanner->source_location.line, parser->scanner->source_location.column);
            ptr_list_append(parser->messages, string_destroy(sb));
            parser->error = true;
            event_cancel_with_errno(node_parsed_ev, errnum);
            json_node_unref(array);
        } else {
            // we finished parsing, so return the floating array
            event_return(node_parsed_ev, array);
        }
        free(ctx);
        return;
    }

    json_array_add_element(array, element);

    // continue parsing the array
    json_scanner_next_async(parser->scanner, node_parsed_ev->loop, json_parse_array_element_scanner_next_cb, ctx);
}

struct parse_object_entry_ctx {
    json_parser *parser;
    json_node *object;
    event *node_parsed_ev;
    char *member_name;          // nullable
    bool has_colon;
    bool has_member_value;
    bool has_trailing_comma;
};

static void parse_object_entry_ctx_free(struct parse_object_entry_ctx *ctx)
{
    free(ctx->member_name);
    free(ctx);
}

static void json_parser_parse_object_entry_cb(const event *ev, void *user_data);

static void json_parser_parse_object_entry_value_cb(const event *ev, void *user_data)
{
    int errnum = 0;
    json_node *member_value = json_parser_parse_node_finish(ev, &errnum);
    struct parse_object_entry_ctx *ctx = user_data;
    json_parser *parser = ctx->parser;
    json_node *object = ctx->object;
    event *node_parsed_ev = ctx->node_parsed_ev;

    if (!member_value) {
        event_cancel_with_errno(node_parsed_ev, errnum);
        json_node_unref(object);
        parse_object_entry_ctx_free(ctx);
        return;
    }

    json_object_set_member(object, ctx->member_name, member_value);

    // save this for the state machine
    ctx->has_member_value = true;

    // scan the next token
    json_scanner_next_async(parser->scanner, node_parsed_ev->loop, json_parser_parse_object_entry_cb, ctx);
}

static void json_parser_parse_object_entry_cb(const event *ev, void *user_data)
{
    int errnum = 0;
    json_token token = json_scanner_next_finish(ev, &errnum);
    struct parse_object_entry_ctx *ctx = user_data;
    json_parser *parser = ctx->parser;
    json_node *object = ctx->object;
    event *node_parsed_ev = ctx->node_parsed_ev;

    if (token == json_token_error) {
        ctx->has_trailing_comma = false;
        event_cancel_with_errno(node_parsed_ev, errnum);
        json_node_unref(object);
        parse_object_entry_ctx_free(ctx);
    } else if (token == json_token_string && !ctx->member_name) {
        ctx->has_trailing_comma = false;
        ctx->member_name = strdup(parser->scanner->last_token_buffer);
        json_scanner_next_async(parser->scanner, node_parsed_ev->loop, json_parser_parse_object_entry_cb, ctx);
    } else if (token == json_token_colon && ctx->member_name && !ctx->has_colon) {
        ctx->has_colon = true;
        json_parser_parse_node_async(parser, node_parsed_ev->loop, json_parser_parse_object_entry_value_cb, ctx);
    } else if (token == json_token_comma && ctx->member_name && ctx->has_colon && ctx->has_member_value) {
        // we want to parse another object entry

        // reset our context for reuse
        free(ctx->member_name);
        ctx->member_name = NULL;
        ctx->has_colon = false;
        ctx->has_member_value = false;
        ctx->has_trailing_comma = true;

        json_scanner_next_async(parser->scanner, node_parsed_ev->loop, json_parser_parse_object_entry_cb, ctx);
    } else if (token == json_token_closebrace && !ctx->has_trailing_comma &&
        (!!ctx->member_name == ctx->has_colon && ctx->has_colon == ctx->has_member_value)) {
        // finish parsing the object
        event_return(node_parsed_ev, object);
        parse_object_entry_ctx_free(ctx);
    } else {
        // unexpected token
        string *sb = string_new();

        string_appendf(sb, "%s:%u:%u: error: unexpected %s (`%s') while parsing JSON object entry",
                parser->scanner->filename,
                parser->scanner->source_location.line, parser->scanner->source_location.column,
                json_token_to_string(token),
                parser->scanner->last_token_buffer);
        ptr_list_append(parser->messages, string_destroy(sb));
        parser->error = true;
        event_cancel_with_errno(node_parsed_ev, EPROTO);
        json_node_unref(object);
        parse_object_entry_ctx_free(ctx);
    }
}

struct next_token_ctx {
    json_parser *parser;
    event *node_parsed_ev;
};

static void json_parser_scanner_next_cb(const event *ev, void *user_data)
{
    struct next_token_ctx *ctx = user_data;
    json_parser *parser = ctx->parser;
    event *node_parsed_ev = ctx->node_parsed_ev;
    int errnum = 0;
    json_token token = json_scanner_next_finish(ev, &errnum);

    free(ctx);
    ctx = NULL;
    if (errnum != 0) {
        event_cancel_with_errno(node_parsed_ev, errnum);
        return;
    }

    switch (token) {
    case json_token_error:
        event_cancel_with_errno(node_parsed_ev, EPROTO);
        break;

    case json_token_eof:
    case json_token_colon:
    case json_token_comma:
    case json_token_closebrace:
    case json_token_closebracket:
    {
        string *sb = string_new();

        string_appendf(sb, "%s:%u:%u: error: unexpected %s (`%s') at start of parsing JSON node",
                parser->scanner->filename,
                parser->scanner->source_location.line, parser->scanner->source_location.column,
                json_token_to_string(token),
                parser->scanner->last_token_buffer);
        ptr_list_append(parser->messages, string_destroy(sb));
        parser->error = true;
        event_cancel_with_errno(node_parsed_ev, EPROTO);
    }   break;

    case json_token_keyword_null:
        event_return(node_parsed_ev, json_null_new());
        break;

    case json_token_keyword_true:
    case json_token_keyword_false:
        event_return(node_parsed_ev, json_boolean_new(token == json_token_keyword_true));
        break;

    case json_token_string:
        event_return(node_parsed_ev, json_string_new(parser->scanner->last_token_buffer));
        break;

    case json_token_integer:
    {
        int64_t value;
        sscanf(parser->scanner->last_token_buffer, "%"PRId64, &value);
        event_return(node_parsed_ev, json_integer_new(value));
    }   break;

    case json_token_double:
        event_return(node_parsed_ev, json_double_new(atof(parser->scanner->last_token_buffer)));
        break;

    case json_token_openbracket:
    {   // parse array
        json_node *array = json_array_new();

        struct parse_array_ctx *parse_array_ctx;
        box(struct parse_array_ctx, parse_array_ctx) {
            parser,
            array,
            node_parsed_ev
        };
        json_parser_parse_node_async(parser, node_parsed_ev->loop, json_parser_parse_array_element_cb, parse_array_ctx);
    }   break;

    case json_token_openbrace:
    {   // parse object
        json_node *object = json_object_new();

        struct parse_object_entry_ctx *parse_ctx;
        box(struct parse_object_entry_ctx, parse_ctx) {
            parser,
            object,
            node_parsed_ev,
            NULL,   /* member_name */
            false,
            false,
            false
        };
        json_scanner_next_async(parser->scanner, node_parsed_ev->loop, json_parser_parse_object_entry_cb, parse_ctx);
    }   break;
    }
}

void json_parser_parse_node_async(json_parser   *parser,
                                  eventloop     *loop,
                                  async_callback callback,
                                  void          *user_data)
{
    if (parser->error) {
        ptr_list_clear(parser->messages);
        parser->error = false;
    }

    event *ev = event_new(callback, user_data);
    eventloop_add(loop, ev);

    struct next_token_ctx *ctx;
    box(struct next_token_ctx, ctx) { parser, ev };
    json_scanner_next_async(parser->scanner, loop, json_parser_scanner_next_cb, ctx);
}

json_node *json_parser_parse_node_finish(const event *ev, int *error)
{
    void *result = NULL;

    if (!event_get_result(ev, &result)) {
        if (error)
            *error = event_get_errno(ev);
        return NULL;
    }

    return result;
}

iterator json_parser_get_messages(json_parser *parser)
{
    iterator parser_messages_iterator = ptr_list_iterator_create(parser->messages);

    if (!parser_messages_iterator.has_next && parser->scanner->message) {
        return (iterator) {
            .data = parser->scanner->message,
            .is_first = true,
            .has_next = true,
            .collection = NULL,
            .iterate = NULL,
            .get_item = NULL,
            .item_maps = { NULL }
        };
    }

    return parser_messages_iterator;
}

void json_parser_destroy(json_parser *parser)
{
    ptr_list_destroy(parser->messages);
    parser->messages = NULL;
    json_scanner_destroy(parser->scanner);
    parser->scanner = NULL;
    free(parser);
}
