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
    lstf_token_assignment,
    lstf_token_equals,
    lstf_token_leftarrow,
    lstf_token_integer,
    lstf_token_double,
    lstf_token_identifier,
    lstf_token_keyword_true,
    lstf_token_keyword_false,
    lstf_token_keyword_null,
    lstf_token_keyword_let,
    lstf_token_keyword_for,
    lstf_token_keyword_of,
    lstf_token_keyword_const,
    lstf_token_string
};
typedef enum _lstf_token lstf_token;

struct _lstf_scanner {
    lstf_token *tokens;
    lstf_sourceloc *token_beginnings;
    lstf_sourceloc *token_endings;
    int current_token_idx;
    int num_tokens;
    int num_errors;
};
typedef struct _lstf_scanner lstf_scanner;

lstf_scanner *lstf_scanner_create(const lstf_file *script);

lstf_token lstf_scanner_next(lstf_scanner *scanner, lstf_sourceloc *const sourceloc_ptr);

lstf_token lstf_scanner_current(const lstf_scanner *scanner);

void lstf_scanner_destroy(lstf_scanner *scanner);
