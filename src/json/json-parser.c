#include "data-structures/iterator.h"
#include "data-structures/ptr-list.h"
#include "data-structures/string-builder.h"
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
    json_parser *parser = json_parser_create_from_stream(inputstream_new_from_const_string(str));

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

iterator json_parser_get_messages(json_parser *parser)
{
    iterator parser_messages_iterator = ptr_list_iterator_create(parser->messages);

    if (!parser_messages_iterator.has_next && parser->scanner->message) {
        return (iterator) {
            .data = { parser->scanner->message, NULL },
            .is_first = true,
            .has_next = true,
            .collection = NULL,
            .iterate = NULL,
            .get_item = NULL
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
