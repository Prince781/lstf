#include "lstf-scanner.h"
#include "compiler/lstf-sourceloc.h"
#include "lstf-report.h"
#include "lstf-sourceref.h"
#include "lstf-file.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#ifdef max
#undef max
#endif
static inline size_t max(size_t a, size_t b)
{
    return a > b ? a : b;
}

const char *lstf_token_to_string(lstf_token token)
{
    switch (token) {
        case lstf_token_assignment:
            return "`='";
        case lstf_token_closebrace:
            return "`}'";
        case lstf_token_closebracket:
            return "`]'";
        case lstf_token_closeparen:
            return "`)'";
        case lstf_token_coalescer:
            return "`\?\?'";
        case lstf_token_colon:
            return "`:'";
        case lstf_token_comma:
            return "`,'";
        case lstf_token_comment:
            return "comment";
        case lstf_token_double:
            return "double";
        case lstf_token_ellipsis:
            return "`...'";
        case lstf_token_eof:
            return "end-of-file";
        case lstf_token_equal:
            return "`=='";
        case lstf_token_notequal:
            return "`!='";
        case lstf_token_identifier:
            return "identifier";
        case lstf_token_integer:
            return "integer";
        case lstf_token_verticalbar:
            return "`|'";
        case lstf_token_ampersand:
            return "`&'";
        case lstf_token_bitwise_xor:
            return "`^'";
        case lstf_token_logical_and:
            return "`&&'";
        case lstf_token_logical_or:
            return "`||'";
        case lstf_token_exclamationpoint:
            return "`!'";
        case lstf_token_tilde:
            return "`~'";
        case lstf_token_leftangle:
            return "`<'";
        case lstf_token_rightangle:
            return "`>'";
        case lstf_token_leftshift:
            return "`<<'";
        case lstf_token_rightshift:
            return "`>>'";
        case lstf_token_lessthan_equal:
            return "`<='";
        case lstf_token_greaterthan_equal:
            return "`>='";
        case lstf_token_plus:
            return "`+'";
        case lstf_token_minus:
            return "`-'";
        case lstf_token_multiply:
            return "`*'";
        case lstf_token_divide:
            return "`/'";
        case lstf_token_exponentiation:
            return "`**'";
        case lstf_token_modulo:
            return "`%'";
        case lstf_token_keyword_await:
            return "`await'";
        case lstf_token_keyword_const:
            return "`const'";
        case lstf_token_keyword_async:
            return "`async'";
        case lstf_token_keyword_false:
            return "`false'";
        case lstf_token_keyword_for:
            return "`for'";
        case lstf_token_keyword_let:
            return "`let'";
        case lstf_token_keyword_null:
            return "`null'";
        case lstf_token_keyword_of:
            return "`of'";
        case lstf_token_keyword_true:
            return "`true'";
        case lstf_token_keyword_enum:
            return "`enum'";
        case lstf_token_keyword_class:
            return "`class'";
        case lstf_token_keyword_fun:
            return "`fun'";
        case lstf_token_keyword_return:
            return "`return'";
        case lstf_token_keyword_in:
            return "`in'";
        case lstf_token_keyword_interface:
            return "`interface'";
        case lstf_token_keyword_extends:
            return "`extends'";
        case lstf_token_keyword_type:
            return "`type'";
        case lstf_token_equivalent:
            return "`<=>'";
        case lstf_token_doublerightarrow:
            return "`=>'";
        case lstf_token_openbrace:
            return "`{'";
        case lstf_token_openbracket:
            return "`['";
        case lstf_token_openparen:
            return "`('";
        case lstf_token_period:
            return "`.'";
        case lstf_token_questionmark:
            return "`?'";
        case lstf_token_semicolon:
            return "`;'";
        case lstf_token_string:
            return "string";
        case lstf_token_error:
            break;
    }

    fprintf(stderr, "%s: unexpected LSTF token `%u'\n",
            __func__, token);
    abort();
}

lstf_scanner *lstf_scanner_create(lstf_file *script)
{
    lstf_scanner *scanner = calloc(1, sizeof *scanner);

    if (!scanner) {
        perror("could not create LSTF scanner");
        exit(EXIT_FAILURE);
    }

    lstf_sourceloc current = { 1, 1, script->content };
    unsigned token_bufsize = 0;
    while (true) {
        lstf_sourceloc begin = current;
        lstf_token current_token = lstf_token_error;

        if (*current.pos && isspace(*current.pos)) {
            if (*current.pos == '\n' || *current.pos == '\r') {
                current.line++;
                current.column = 1;
            } else {
                current.column++;
            }
            current.pos++;
            continue;
        }

        switch (*current.pos) {
        case '{':
            current_token = lstf_token_openbrace;
            current.pos++;
            current.column++;
            break;
        case '}':
            current_token = lstf_token_closebrace;
            current.pos++;
            current.column++;
            break;
        case '[':
            current_token = lstf_token_openbracket;
            current.pos++;
            current.column++;
            break;
        case ']':
            current_token = lstf_token_closebracket;
            current.pos++;
            current.column++;
            break;
        case '(':
            current_token = lstf_token_openparen;
            current.pos++;
            current.column++;
            break;
        case ')':
            current_token = lstf_token_closeparen;
            current.pos++;
            current.column++;
            break;
        case '?':
            current_token = lstf_token_questionmark;
            current.pos++;
            current.column++;
            if (*current.pos == '?') {
                current_token = lstf_token_coalescer;
                current.pos++;
                current.column++;
            }
            break;
        case ':':
            current_token = lstf_token_colon;
            current.pos++;
            current.column++;
            break;
        case ';':
            current_token = lstf_token_semicolon;
            current.pos++;
            current.column++;
            break;
        case ',':
            current_token = lstf_token_comma;
            current.pos++;
            current.column++;
            break;
        case '.':
            current_token = lstf_token_period;
            current.pos++;
            current.column++;
            if (*current.pos == '.' && *(current.pos + 1) == '.') {
                current_token = lstf_token_ellipsis;
                current.pos += 2;
                current.column += 2;
            }
            break;
        case '=':
            current_token = lstf_token_assignment;
            current.pos++;
            current.column++;
            if (*current.pos == '=') {
                current_token = lstf_token_equal;
                current.pos++;
                current.column++;
            } else if (*current.pos == '>') {
                current_token = lstf_token_doublerightarrow;
                current.pos++;
                current.column++;
            }
            break;
        case '<':
            current_token = lstf_token_leftangle;
            current.pos++;
            current.column++;
            if (*current.pos == '=') {
                current_token = lstf_token_lessthan_equal;
                current.pos++;
                current.column++;
                if (*current.pos == '>') {
                    current_token = lstf_token_equivalent;
                    current.pos++;
                    current.column++;
                }
            } else if (*current.pos == '<') {
                current_token = lstf_token_leftshift;
                current.pos++;
                current.column++;
            }
            break;
        case '>':
            current_token = lstf_token_rightangle;
            current.pos++;
            current.column++;
            if (*current.pos == '=') {
                current_token = lstf_token_greaterthan_equal;
                current.pos++;
                current.column++;
            } else if (*current.pos == '>') {
                current_token = lstf_token_rightshift;
                current.pos++;
                current.column++;
            }
            break;
        case '|':
            current_token = lstf_token_verticalbar;
            current.pos++;
            current.column++;
            if (*current.pos == '|') {
                current_token = lstf_token_logical_or;
                current.pos++;
                current.column++;
            }
            break;
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            if (*current.pos == '-')
                current_token = lstf_token_minus;
            else
                current_token = lstf_token_integer;
            current.pos++;
            current.column++;
            while (*current.pos && isdigit(*current.pos)) {
                current.pos++;
                current.column++;
                current_token = lstf_token_integer;
            }
            if (*current.pos == '.') {
                current_token = lstf_token_error;
                current.pos++;
                current.column++;
                if (*current.pos && isdigit(*current.pos)) {
                    current_token = lstf_token_double;
                    current.pos++;
                    current.column++;
                    while (*current.pos && isdigit(*current.pos)) {
                        current.pos++;
                        current.column++;
                    }
                    if (*current.pos == 'e' || *current.pos == 'E') {
                        current_token = lstf_token_error;
                        current.pos++;
                        current.column++;
                        if (*current.pos == '+' || *current.pos == '-') {
                            current.pos++;
                            current.column++;
                            if (*current.pos && isdigit(*current.pos)) {
                                current_token = lstf_token_double;
                                current.pos++;
                                current.column++;
                                while (*current.pos && isdigit(*current.pos)) {
                                    current.pos++;
                                    current.column++;
                                }
                            } else {
                                lstf_report_error(&lstf_sourceref_at_location(script, begin), "expected exponent");
                                current_token = lstf_token_error;
                                scanner->num_errors++;
                            }
                        } else {
                            lstf_report_error(&lstf_sourceref_at_location(script, begin), "expected exponent");
                            current_token = lstf_token_error;
                            scanner->num_errors++;
                        }
                    }
                } else {
                    lstf_report_error(&lstf_sourceref_at_location(script, begin), "expected fractional part");
                    current_token = lstf_token_error;
                    scanner->num_errors++;
                }
            }
            break;
        case '\'':
        case '"':
            current.pos++;
            current.column++;
            while (*current.pos && *current.pos != *begin.pos) {
                if (*current.pos == '\\') {
                    if (*(current.pos + 1) && !(*(current.pos + 1) == '\n' || *(current.pos + 1) == '\r')) {
                        current.pos++;
                        current.column++;
                    } else
                        break;
                }
                if (*current.pos == '\n' || *current.pos == '\r')
                    break;
                current.pos++;
                current.column++;
            }
            if (*current.pos != *begin.pos) {
                lstf_report_error((&(lstf_sourceref){script, begin, current}), "unterminated string");
                lstf_report_note(&lstf_sourceref_at_location(script, begin), "unterminated string begins here");
                current_token = lstf_token_error;
                scanner->num_errors++;
            } else {
                current_token = lstf_token_string;
                current.pos++;
                current.column++;
            }
            break;
        case '/':
            // skip comment
            if (*(current.pos + 1) == '/') {
                current.pos++;
                current.column++;
                while (*current.pos && !(*current.pos == '\n' || *current.pos == '\r')) {
                    current.pos++;
                    current.column++;
                }
            } else if (*(current.pos + 1) == '*') {
                current.pos++;
                current.column++;
                while (*current.pos && !(*current.pos == '*' && *(current.pos + 1) == '/')) {
                    if (*current.pos == '\n' || *current.pos == '\r') {
                        current.line++;
                        current.column = 1;
                    } else {
                        current.column++;
                    }
                    current.pos++;
                }
                if (!*current.pos) {
                    lstf_report_error(&lstf_sourceref_at_location(script, begin), "unterminated multi-line comment");
                    scanner->num_errors++;
                } else {
                    current.pos += 2;
                    current.column += 2;
                }
            } else {
                current_token = lstf_token_divide;
                current.pos++;
                current.column++;
            }
            break;
        case '^':
            current_token = lstf_token_bitwise_xor;
            current.pos++;
            current.column++;
            break;
        case '&':
            current_token = lstf_token_ampersand;
            current.pos++;
            current.column++;
            if (*current.pos == '&') {
                current_token = lstf_token_logical_and;
                current.pos++;
                current.column++;
            }
            break;
        case '+':
            current_token = lstf_token_plus;
            current.pos++;
            current.column++;
            break;
        case '%':
            current_token = lstf_token_modulo;
            current.pos++;
            current.column++;
            break;
        case '!':
            current_token = lstf_token_exclamationpoint;
            current.pos++;
            current.column++;
            if (*current.pos == '=') {
                current_token = lstf_token_notequal;
                current.pos++;
                current.column++;
            }
            break;
        case '~':
            current_token = lstf_token_tilde;
            current.pos++;
            current.column++;
            break;
        case '*':
            current_token = lstf_token_multiply;
            current.pos++;
            current.column++;
            break;
        case '\0':
            current_token = lstf_token_eof;
            break;
        default:
            if (*current.pos && (isalpha(*current.pos) || *current.pos == '_')) {
                current_token = lstf_token_identifier;
                while (isalnum(*current.pos) || *current.pos == '_') {
                    current.pos++;
                    current.column++;
                }
                if (strncmp(begin.pos, "async", max(sizeof "async" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_async;
                else if (strncmp(begin.pos, "true", max(sizeof "true" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_true;
                else if (strncmp(begin.pos, "false", max(sizeof "false" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_false;
                else if (strncmp(begin.pos, "null", max(sizeof "null" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_null;
                else if (strncmp(begin.pos, "let", max(sizeof "let" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_let;
                else if (strncmp(begin.pos, "for", max(sizeof "for" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_for;
                else if (strncmp(begin.pos, "of", max(sizeof "of" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_of;
                else if (strncmp(begin.pos, "const", max(sizeof "const" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_const;
                else if (strncmp(begin.pos, "as", max(sizeof "as" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_async;
                else if (strncmp(begin.pos, "await", max(sizeof "await" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_await;
                else if (strncmp(begin.pos, "enum", max(sizeof "enum" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_enum;
                else if (strncmp(begin.pos, "class", max(sizeof "class" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_class;
                else if (strncmp(begin.pos, "fun", max(sizeof "fun" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_fun;
                else if (strncmp(begin.pos, "in", max(sizeof "in" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_in;
                else if (strncmp(begin.pos, "interface", max(sizeof "interface" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_interface;
                else if (strncmp(begin.pos, "extends", max(sizeof "extends" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_extends;
                else if (strncmp(begin.pos, "type", max(sizeof "type" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_type;
                else if (strncmp(begin.pos, "return", max(sizeof "return" - 1, current.pos - begin.pos)) == 0)
                    current_token = lstf_token_keyword_return;
            } else {
                lstf_report_error(&lstf_sourceref_at_location(script, begin), "unexpected token `%c'", *current.pos);
                current.pos++;
                current.column++;
                current_token = lstf_token_error;
                scanner->num_errors++;
            }
            break;
        }

        lstf_sourceloc end = current;
        if (!(current_token == lstf_token_eof || current_token == lstf_token_error)) {
            end.pos--;
            end.column--;
        }

        // add token to list
        if (!(current_token == lstf_token_error || current_token == lstf_token_comment)) {
            if (scanner->num_tokens >= token_bufsize) {
                if (token_bufsize == 0)
                    token_bufsize = 64;
                else
                    token_bufsize *= 2;

                if (!(scanner->tokens = realloc(scanner->tokens, token_bufsize * sizeof *scanner->tokens))) {
                    perror("could not create token buffer");
                    abort();
                }
                if (!(scanner->token_beginnings = realloc(scanner->token_beginnings, 
                                token_bufsize * sizeof *scanner->token_beginnings))) {
                    perror("could not create token buffer");
                    abort();
                }
                if (!(scanner->token_endings = realloc(scanner->token_endings,
                                token_bufsize * sizeof *scanner->token_endings))) {
                    perror("could not create token buffer");
                    abort();
                }
            }

            scanner->tokens[scanner->num_tokens] = current_token;
            scanner->token_beginnings[scanner->num_tokens] = begin;
            scanner->token_endings[scanner->num_tokens] = end;
            scanner->num_tokens++;
        }

        if (current_token == lstf_token_eof)
            break;
    }

    script->total_lines = current.line;

    return scanner;
}

lstf_token lstf_scanner_next(lstf_scanner *scanner)
{
    assert(scanner->current_token_idx + 1 < scanner->num_tokens);

    return scanner->tokens[++scanner->current_token_idx];
}

lstf_token lstf_scanner_peek_next(const lstf_scanner *scanner)
{
    assert(scanner->current_token_idx + 1 < scanner->num_tokens);

    return scanner->tokens[scanner->current_token_idx + 1];
}

lstf_token lstf_scanner_current(const lstf_scanner *scanner)
{
    assert(scanner->current_token_idx < scanner->num_tokens);

    return scanner->tokens[scanner->current_token_idx];
}

char *lstf_scanner_get_current_string(const lstf_scanner *scanner) {
    assert(scanner->current_token_idx < scanner->num_tokens);

    return lstf_sourceref_get_string(scanner->token_beginnings[scanner->current_token_idx], 
            scanner->token_endings[scanner->current_token_idx]);
}

lstf_token lstf_scanner_rewind(lstf_scanner *scanner, unsigned position)
{
    assert(position < scanner->num_tokens);

    return scanner->tokens[scanner->current_token_idx = position];
}

lstf_sourceloc lstf_scanner_get_location(const lstf_scanner *scanner)
{
    assert(scanner->current_token_idx < scanner->num_tokens);

    return scanner->token_beginnings[scanner->current_token_idx];
}

lstf_sourceloc lstf_scanner_get_end_location(const lstf_scanner *scanner)
{
    assert(scanner->current_token_idx < scanner->num_tokens);

    return scanner->token_endings[scanner->current_token_idx];
}

lstf_sourceloc lstf_scanner_get_prev_end_location(const lstf_scanner *scanner)
{
    assert(scanner->current_token_idx > 0);

    return scanner->token_endings[scanner->current_token_idx - 1];
}

void lstf_scanner_destroy(lstf_scanner *scanner)
{
    free(scanner->token_beginnings);
    scanner->token_beginnings = NULL;
    free(scanner->token_endings);
    scanner->token_endings = NULL;
    free(scanner->tokens);
    scanner->tokens = NULL;
    scanner->num_tokens = 0;
    free(scanner);
}
