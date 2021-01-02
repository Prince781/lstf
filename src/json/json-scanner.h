#pragma once

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
    json_token_keyword_null
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

const char *json_scanner_get_message(json_scanner *scanner);

void json_scanner_destroy(json_scanner *scanner);
