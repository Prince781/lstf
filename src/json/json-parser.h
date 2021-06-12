#pragma once

#include "json.h"
#include "json-scanner.h"
#include "data-structures/iterator.h"
#include "io/inputstream.h"
#include "io/event.h"
#include <stdio.h>
#include <stdbool.h>

struct _ptr_list; // see ptr-list.h
typedef struct _ptr_list ptr_list;

struct _string_builder;
typedef struct _string_builder string_builder;

struct _json_parser {
    /**
     * The underlying scanner for this parser.
     */
    json_scanner *scanner;

    /**
     * Whether the parser has hit an error.
     */
    bool error;

    /**
     * Last message(s) from the parser. May be empty 
     * if there is a scanner error. Use json_parser_get_message().
     */
    ptr_list *messages;
};
typedef struct _json_parser json_parser;

/**
 * Constructs a new JSON node from a string.
 */
json_node *json_parser_parse_string(const char *str);

/**
 * Creates a streaming parser.
 */
json_parser *json_parser_create_from_stream(inputstream *stream);

/**
 * Parse a single node. Returns NULL if the stream hits EOF.
 * Otherwise, returns a floating JSON node.
 */
json_node *json_parser_parse_node(json_parser *parser);

/**
 * Parses a single node asynchronously. Returns NULL in the callback if the
 * stream hits EOF. Otherwise, returns a floating JSON node.
 *
 * @see json_parser_parse_node
 */
void json_parser_parse_node_async(json_parser   *parser,
                                  eventloop     *loop,
                                  async_callback callback,
                                  void          *user_data);

/**
 * Completes parsing of a single node.
 *
 * @see json_parser_parse_node_async
 */
json_node *json_parser_parse_node_finish(event *ev, int *error);

/**
 * Returns an iterator on of (char *) messages either from the parser
 * or the scanner.
 */
iterator json_parser_get_messages(json_parser *parser);

void json_parser_destroy(json_parser *parser);
