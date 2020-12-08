#pragma once

#include "lstf-sourceloc.h"
#include "lstf-file.h"

enum _lstf_token {
    lstf_token_eof,
    lstf_token_error,
    lstf_token_comment,
    lstf_token_openbrace,
    lstf_token_closebrace,
    lstf_token_openbracket,
    lstf_token_closebracket,
    lstf_token_openparen,
    lstf_token_closeparen,
    lstf_token_questionmark,
    lstf_token_colon,
    lstf_token_semicolon,
    lstf_token_comma,
    lstf_token_period,
    lstf_token_ellipsis,
    lstf_token_assignment,
    lstf_token_equals,
    lstf_token_equivalent,
    lstf_token_doublerightarrow,
    lstf_token_integer,
    lstf_token_double,
    lstf_token_identifier,
    lstf_token_verticalbar,
    lstf_token_keyword_await,
    lstf_token_keyword_true,
    lstf_token_keyword_false,
    lstf_token_keyword_null,
    lstf_token_keyword_let,
    lstf_token_keyword_for,
    lstf_token_keyword_of,
    lstf_token_keyword_const,
    lstf_token_keyword_async,
    lstf_token_keyword_enum,
    lstf_token_keyword_class,
    lstf_token_keyword_fun,
    lstf_token_keyword_return,
    lstf_token_keyword_interface,
    lstf_token_keyword_extends,
    lstf_token_keyword_type,
    lstf_token_string
};
typedef enum _lstf_token lstf_token;

const char *lstf_token_to_string(lstf_token token);

struct _lstf_scanner {
    lstf_token *tokens;
    lstf_sourceloc *token_beginnings;
    lstf_sourceloc *token_endings;
    unsigned current_token_idx;
    unsigned num_tokens;
    unsigned num_errors;
};
typedef struct _lstf_scanner lstf_scanner;

lstf_scanner *lstf_scanner_create(lstf_file *script);

/**
 * Advance the token pointer and returns next token. Returns error if there are no tokens.
 * If [begin] is non-NULL, it is filled with the beginning of the current 
 */
lstf_token lstf_scanner_next(lstf_scanner *scanner);

lstf_token lstf_scanner_peek_next(const lstf_scanner *scanner);

lstf_token lstf_scanner_current(const lstf_scanner *scanner);

char *lstf_scanner_get_current_string(const lstf_scanner *scanner);

lstf_token lstf_scanner_rewind(lstf_scanner *scanner, unsigned position);

/**
 * Gets the start of the current token.
 */
lstf_sourceloc lstf_scanner_get_location(const lstf_scanner *scanner);

/**
 * Gets the end of the previous token.
 */
lstf_sourceloc lstf_scanner_get_prev_end_location(const lstf_scanner *scanner);

void lstf_scanner_destroy(lstf_scanner *scanner);
