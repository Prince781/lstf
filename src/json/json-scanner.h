#pragma once

#include "io/event.h"
#include "io/inputstream.h"
#include <stdio.h>
#include <stdbool.h>

enum _json_token {
    json_token_eof,
    json_token_error,
    json_token_openbrace,
    json_token_closebrace,
    json_token_openbracket,
    json_token_closebracket,
    json_token_colon,
    json_token_comma,
    json_token_integer,
    json_token_double,
    json_token_string,
    json_token_keyword_true,
    json_token_keyword_false,
    json_token_keyword_null,
    json_token_pattern_ellipsis
};
typedef enum _json_token json_token;

const char *json_token_to_string(json_token token);

struct _json_sourceloc {
    unsigned line;
    unsigned column;
    const char *pos;
};
typedef struct _json_sourceloc json_sourceloc;

struct _json_scanner {
    json_sourceloc source_location;
    json_sourceloc prev_char_source_location;
    json_token last_token;
    char *last_token_buffer;
    json_sourceloc last_token_begin;
    unsigned last_token_length;
    unsigned last_token_buffer_size;
    inputstream *stream;
    char *filename;
    char *message;
};
typedef struct _json_scanner json_scanner;

/**
 * Creates a scanner from a `FILE` stream.
 */
json_scanner *json_scanner_create_from_stream(inputstream *stream);

json_token json_scanner_next(json_scanner *scanner);

void json_scanner_next_async(json_scanner  *scanner,
                             eventloop     *loop,
                             async_callback callback,
                             void          *user_data);

json_token json_scanner_next_finish(const event *ev, int *error);

/**
 * Resets the source locations
 */
static inline void json_scanner_reset(json_scanner *scanner) {
    scanner->source_location = (json_sourceloc){ 0 };
    scanner->prev_char_source_location = (json_sourceloc){ 0 };
    scanner->last_token = json_token_eof;
    scanner->last_token_begin = (json_sourceloc){ 0 };
}

void json_scanner_destroy(json_scanner *scanner);
