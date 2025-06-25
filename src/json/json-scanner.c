#include "json-scanner.h"
#include "data-structures/string-builder.h"
#include "io/event.h"
#include "io/inputstream.h"
#include "json/json-parser.h"
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *json_token_to_string(json_token token)
{
    switch (token) {
    case json_token_eof:
        return "EOF";
    case json_token_error:
        return "";
    case json_token_openbrace:
        return "open brace";
    case json_token_closebrace:
        return "close brace";
    case json_token_openbracket:
        return "open bracket";
    case json_token_closebracket:
        return "close bracket";
    case json_token_colon:
        return "colon";
    case json_token_comma:
        return "comma";
    case json_token_integer:
        return "integer";
    case json_token_double:
        return "double";
    case json_token_string:
        return "string";
    case json_token_keyword_true:
    case json_token_keyword_false:
        return "boolean";
    case json_token_keyword_null:
        return "null keyword";
    case json_token_pattern_ellipsis:
        return "ellipsis";
    }

    fprintf(stderr, "%s: unexpected value `%u' for json_token\n", __func__, token);
    abort();
}

json_scanner *json_scanner_create_from_stream(inputstream *stream)
{
    json_scanner *scanner = NULL;

    if (!stream)
        return NULL;

    if (!(scanner = calloc(1, sizeof *scanner))) {
        perror("could not create JSON scanner");
        exit(EXIT_FAILURE);
    }

    scanner->stream = inputstream_ref(stream);
    scanner->filename = inputstream_get_name(stream);
    scanner->source_location.line = 1;

    return scanner;
}

static void json_scanner_save_char(json_scanner *scanner, char read_character)
{
    if (scanner->last_token_length + 1 >= scanner->last_token_buffer_size) {
        if (scanner->last_token_buffer_size == 0)
            scanner->last_token_buffer_size = 64;
        else
            scanner->last_token_buffer_size *= 2;
        char *last_token_buffer = realloc(scanner->last_token_buffer, scanner->last_token_buffer_size);
        if (!last_token_buffer) {
            perror("could not allocate buffer for last token");
            abort();
        }
        scanner->last_token_buffer = last_token_buffer;
    }

    scanner->last_token_buffer[scanner->last_token_length++] = read_character;
    scanner->last_token_buffer[scanner->last_token_length] = '\0';
}

static void json_scanner_save_string(json_scanner *scanner, const char *read_characters)
{
    for (const char *p = read_characters; *p; ++p)
        json_scanner_save_char(scanner, *p);
}

static int xvalue(int digit_character)
{
    switch (digit_character) {
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
            return digit_character - '0';
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
            return 0xA + (digit_character - 'a');
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
            return 0xA + (digit_character - 'A');
        default:
            return 0xFF;
    }
}

static int json_scanner_getc(json_scanner *scanner)
{
    char read_character;
    if (!inputstream_read_char(scanner->stream, &read_character))
        return EOF;

    scanner->prev_char_source_location = scanner->source_location;
    if (read_character == '\n' || read_character == '\r') {
        scanner->source_location.line++;
        scanner->source_location.column = 0;
    } else {
        scanner->source_location.column++;
    }

    return read_character;
}

static void json_scanner_ungetc(json_scanner *scanner, char read_character)
{
    if (inputstream_unread_char(scanner->stream, read_character))
        scanner->source_location = scanner->prev_char_source_location;
    else
        fprintf(stderr, "%s: failed: %s\n", __func__, strerror(errno));
}


__attribute__ ((format (printf, 3, 4)))
static void 
json_scanner_report_message(json_scanner *scanner, json_sourceloc source_location, const char *format, ...)
{
    string *sb = string_new();
    string_appendf(sb, "%s:%u:%u: ",
            scanner->filename, 
            source_location.line, source_location.column);
    va_list args;
    va_start(args, format);
    string_append_va(sb, format, args);
    va_end(args);
    scanner->message = string_destroy(sb);
}

json_token json_scanner_next(json_scanner *scanner)
{
    int current_char = json_scanner_getc(scanner);
    if (current_char == EOF)
        return scanner->last_token = json_token_eof;

    if (scanner->message) {
        free(scanner->message);
        scanner->message = NULL;
    }

    scanner->last_token_length = 0;
    if (scanner->last_token_buffer)
        scanner->last_token_buffer[scanner->last_token_length] = '\0';

    while (isspace(current_char))
        current_char = json_scanner_getc(scanner);

    scanner->last_token_begin = scanner->source_location;
    switch (current_char) {
    case '{':
        json_scanner_save_char(scanner, current_char);
        return scanner->last_token = json_token_openbrace;
    case '}':
        json_scanner_save_char(scanner, current_char);
        return scanner->last_token = json_token_closebrace;
    case '[':
        json_scanner_save_char(scanner, current_char);
        return scanner->last_token = json_token_openbracket;
    case ']':
        json_scanner_save_char(scanner, current_char);
        return scanner->last_token = json_token_closebracket;
    case ':':
        json_scanner_save_char(scanner, current_char);
        return scanner->last_token = json_token_colon;
    case ',':
        json_scanner_save_char(scanner, current_char);
        return scanner->last_token = json_token_comma;
    case 't':
    {
        if (json_scanner_getc(scanner) == 'r' &&
                json_scanner_getc(scanner) == 'u' &&
                json_scanner_getc(scanner) == 'e') {
            json_scanner_save_string(scanner, "true");
            return scanner->last_token = json_token_keyword_true;
        }
        json_scanner_report_message(scanner, scanner->source_location, "expected `true'");
        return scanner->last_token = json_token_error;
    }
    case 'f':
    {
        if (json_scanner_getc(scanner) == 'a' &&
                json_scanner_getc(scanner) == 'l' &&
                json_scanner_getc(scanner) == 's' &&
                json_scanner_getc(scanner) == 'e') {
            json_scanner_save_string(scanner, "false");
            return scanner->last_token = json_token_keyword_false;
        }
        json_scanner_report_message(scanner, scanner->source_location, "expected `false'");
        return scanner->last_token = json_token_error;
    }
    case 'n':
    {
        if (json_scanner_getc(scanner) == 'u' &&
                json_scanner_getc(scanner) == 'l' &&
                json_scanner_getc(scanner) == 'l') {
            json_scanner_save_string(scanner, "null");
            return scanner->last_token = json_token_keyword_null;
        }
        json_scanner_report_message(scanner, scanner->source_location, "expected `null'");
        return scanner->last_token = json_token_error;
    }
    case '"':
    {
        json_sourceloc begin_sourceloc = scanner->source_location;
        while ((current_char = json_scanner_getc(scanner)) != '"' && current_char != EOF) {
            char previous_char = current_char;
            if (current_char == '\\') {
                current_char = json_scanner_getc(scanner);
                char next_char = 0;
                switch (current_char) {
                case '"':
                case '\\':
                case '/':
                    json_scanner_save_char(scanner, current_char);
                    break;
                case 'b':
                    json_scanner_save_char(scanner, '\b');
                    break;
                case 'f':
                    json_scanner_save_char(scanner, '\f');
                    break;
                case 'n':
                    json_scanner_save_char(scanner, '\n');
                    break;
                case 'r':
                    json_scanner_save_char(scanner, '\r');
                    break;
                case 't':
                    json_scanner_save_char(scanner, '\t');
                    break;
                case 'u':
                    // parse 4 hex digits
                    if (!isxdigit(current_char = json_scanner_getc(scanner)))
                        return json_token_error;
                    if (!isxdigit(next_char = json_scanner_getc(scanner)))
                        return json_token_error;
                    json_scanner_save_char(scanner, xvalue(current_char) << 8 | xvalue(next_char));
                    if (!isxdigit(current_char = json_scanner_getc(scanner)))
                        return json_token_error;
                    if (!isxdigit(next_char = json_scanner_getc(scanner)))
                        return json_token_error;
                    json_scanner_save_char(scanner, xvalue(current_char) << 8 | xvalue(next_char));
                    current_char = next_char;
                    break;
                default:
                    json_scanner_save_char(scanner, previous_char);
                    json_scanner_save_char(scanner, current_char);
                    break;
                }
            } else {
                json_scanner_save_char(scanner, current_char);
            }
        }
        if (current_char == EOF) {
            json_scanner_report_message(scanner, begin_sourceloc, "error: unterminated string");
            return scanner->last_token = json_token_error;
        }
        return scanner->last_token = json_token_string;
    }
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
        json_scanner_save_char(scanner, current_char);
        while (isdigit(current_char = json_scanner_getc(scanner)))
            json_scanner_save_char(scanner, current_char);
        scanner->last_token = json_token_integer;
        if (current_char == '.') {
            json_scanner_save_char(scanner, current_char);
            scanner->last_token = json_token_error;
            if (isdigit(current_char = json_scanner_getc(scanner))) {
                json_scanner_save_char(scanner, current_char);
                scanner->last_token = json_token_double;
                while (isdigit(current_char = json_scanner_getc(scanner)))
                    json_scanner_save_char(scanner, current_char);
                if (current_char == 'e' || current_char == 'E') {
                    json_scanner_save_char(scanner, current_char);
                    scanner->last_token = json_token_error;
                    current_char = json_scanner_getc(scanner);
                    if (current_char == '+' || current_char == '-') {
                        json_scanner_save_char(scanner, current_char);
                        if (isdigit(current_char = json_scanner_getc(scanner))) {
                            json_scanner_save_char(scanner, current_char);
                            scanner->last_token = json_token_double;
                            while (isdigit(current_char = json_scanner_getc(scanner)))
                                json_scanner_save_char(scanner, current_char);
                            json_scanner_ungetc(scanner, current_char);
                        } else {
                            json_scanner_report_message(scanner, scanner->source_location,
                                    "error: expected exponent");
                        }
                    } else {
                        json_scanner_report_message(scanner, scanner->source_location,
                                "error: expected exponent");
                    }
                } else {
                    json_scanner_ungetc(scanner, current_char);
                }
            } else {
                json_scanner_report_message(scanner, scanner->source_location,
                        "error: expected fractional part for number");
            }
        } else {
            json_scanner_ungetc(scanner, current_char);
        }
        return scanner->last_token;
    case '.': 
    {
        if (json_scanner_getc(scanner) == '.' &&
            json_scanner_getc(scanner) == '.') {
            json_scanner_save_string(scanner, "...");
            return scanner->last_token = json_token_pattern_ellipsis;
        }
        json_scanner_report_message(scanner, scanner->source_location,
                                    "expected `...'");
        return scanner->last_token = json_token_error;
    }
    case EOF:
        return scanner->last_token = json_token_eof;
    default:
    {
        string *sb = string_new();
        string_appendf(sb, "%s:%u:%u: error: unexpected character `%c'",
                scanner->filename,
                scanner->source_location.line, scanner->source_location.column,
                current_char);
        scanner->message = string_destroy(sb);
        json_scanner_report_message(scanner, scanner->source_location, 
                "error: unexpected character `%c'", current_char);
        json_scanner_save_char(scanner, current_char);
        return scanner->last_token = json_token_error;
    }
    }

    return scanner->last_token = json_token_eof;
}

/**
 * Waits on a blocking stream. Should not be called with a stream backed by a
 * buffer.
 */
static void json_scanner_stream_wait_async(json_scanner  *scanner,
                                           eventloop     *loop,
                                           async_callback callback,
                                           void          *user_data)
{
    int fd = inputstream_get_fd(scanner->stream);

    if (fd == -1 || inputstream_ready(scanner->stream)) {
        // invoke the callback immediately
        event tmp_ev = { .loop = loop };
        event_return(&tmp_ev, NULL);
        callback(&tmp_ev, user_data);
    } else {
        eventloop_add_fd(loop, fd, true, callback, user_data);
    }
}

struct token_read_ctx {
    json_scanner *scanner;
    event *token_read_ev;
    enum {
        token_read_state_skip_spaces,
        token_read_state_begin,

        token_read_state_number,
        token_read_state_fraction,
        token_read_state_exponent_begin,        // reading after 'E' or 'e'
        token_read_state_exponent,              // reading after '+' or '-'
        token_read_state_exponent_continue,     // continuing to parse a valid exponent

        token_read_state_string,
        token_read_state_string_escaped,        // after reading a '\\'

        token_read_state_true_r,
        token_read_state_true_u,
        token_read_state_true_e,

        token_read_state_false_a,
        token_read_state_false_l,
        token_read_state_false_s,
        token_read_state_false_e,

        token_read_state_null_u,
        token_read_state_null_l1,
        token_read_state_null_l2,
    } state;
    bool is_init_state;
};

static void json_scanner_stream_ready_cb(const event *ev, void *user_data)
{
    struct token_read_ctx *ctx = user_data;
    json_scanner *scanner = ctx->scanner;
    event *token_read_ev = ctx->token_read_ev;

    do {
        if (event_get_result(ev, NULL)) {
            int read_character = json_scanner_getc(scanner);

            if (ctx->is_init_state) {
                // reset the saved token
                scanner->last_token_length = 0;
                if (scanner->last_token_buffer)
                    scanner->last_token_buffer[scanner->last_token_length] = '\0';
                ctx->is_init_state = false;
            }

            if (ctx->state == token_read_state_skip_spaces && !isspace(read_character)) {
                // immediately transition to the next state to handle this non-space character
                ctx->state = token_read_state_begin;
            }

            switch (ctx->state) {
            case token_read_state_skip_spaces:
                // loop 
                if (inputstream_ready(scanner->stream))
                    continue;
                json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
                break;

            case token_read_state_begin:
            {
                scanner->last_token_begin = scanner->source_location;

                switch (read_character) {
                case '{':
                    json_scanner_save_char(scanner, read_character);
                    event_return(token_read_ev, (void *)(scanner->last_token = json_token_openbrace));
                    free(ctx);
                    break;

                case '}':
                    json_scanner_save_char(scanner, read_character);
                    event_return(token_read_ev, (void *)(scanner->last_token = json_token_closebrace));
                    free(ctx);
                    break;

                case '[':
                    json_scanner_save_char(scanner, read_character);
                    event_return(token_read_ev, (void *)(scanner->last_token = json_token_openbracket));
                    free(ctx);
                    break;

                case ']':
                    json_scanner_save_char(scanner, read_character);
                    event_return(token_read_ev, (void *)(scanner->last_token = json_token_closebracket));
                    free(ctx);
                    break;

                case ':':
                    json_scanner_save_char(scanner, read_character);
                    event_return(token_read_ev, (void *)(scanner->last_token = json_token_colon));
                    free(ctx);
                    break;

                case ',':
                    json_scanner_save_char(scanner, read_character);
                    event_return(token_read_ev, (void *)(scanner->last_token = json_token_comma));
                    free(ctx);
                    break;

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
                    ctx->state = token_read_state_number;
                    json_scanner_save_char(scanner, read_character);
                    if (inputstream_ready(scanner->stream))
                        continue;
                    json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
                    break;

                case '.':
                    ctx->state = token_read_state_fraction;
                    json_scanner_save_char(scanner, read_character);
                    if (inputstream_ready(scanner->stream))
                        continue;
                    json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
                    break;

                case '"':
                    ctx->state = token_read_state_string;
                    if (inputstream_ready(scanner->stream))
                        continue;
                    json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
                    break;

                case 't':
                    ctx->state = token_read_state_true_r;
                    json_scanner_save_char(scanner, read_character);
                    if (inputstream_ready(scanner->stream))
                        continue;
                    json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
                    break;

                case 'f':
                    ctx->state = token_read_state_false_a;
                    json_scanner_save_char(scanner, read_character);
                    if (inputstream_ready(scanner->stream))
                        continue;
                    json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
                    break;

                case 'n':
                    ctx->state = token_read_state_null_u;
                    json_scanner_save_char(scanner, read_character);
                    if (inputstream_ready(scanner->stream))
                        continue;
                    json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
                    break;

                case EOF:
                    event_return(token_read_ev, (void *)(scanner->last_token = json_token_eof));
                    free(ctx);
                    break;

                default:
                    json_scanner_report_message(scanner, scanner->source_location,
                            "unexpected character `%c' at token begin", read_character);
                    event_cancel_with_errno(token_read_ev, EPROTO);
                    free(ctx);
                    break;
                }
            }   break;

            case token_read_state_number:
            {
                switch (read_character) {
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
                    json_scanner_save_char(scanner, read_character);
                    if (inputstream_ready(scanner->stream))
                        continue;
                    json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
                    break;

                case '.':
                    ctx->state = token_read_state_fraction;
                    json_scanner_save_char(scanner, read_character);
                    if (inputstream_ready(scanner->stream))
                        continue;
                    json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
                    break;

                default:
                    json_scanner_ungetc(scanner, read_character);
                    event_return(token_read_ev, (void *)(scanner->last_token = json_token_integer));
                    free(ctx);
                    return;
                }
            }   break;

            case token_read_state_fraction:
            {
                switch (read_character) {
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
                    json_scanner_save_char(scanner, read_character);
                    if (inputstream_ready(scanner->stream))
                        continue;
                    json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
                    break;

                case 'e':
                case 'E':
                    ctx->state = token_read_state_exponent_begin;
                    json_scanner_save_char(scanner, read_character);
                    if (inputstream_ready(scanner->stream))
                        continue;
                    json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
                    break;

                default:
                    json_scanner_ungetc(scanner, read_character);
                    event_return(token_read_ev, (void *)(scanner->last_token = json_token_double));
                    free(ctx);
                    return;
                }
            }   break;

            case token_read_state_exponent_begin:
            {
                if (read_character == '+' || read_character == '-' || isdigit(read_character)) {
                    if (isdigit(read_character))
                        ctx->state = token_read_state_exponent_continue;
                    else
                        ctx->state = token_read_state_exponent;
                    json_scanner_save_char(scanner, read_character);
                    if (inputstream_ready(scanner->stream))
                        continue;
                    json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
                } else {
                    // unexpected character (where we wanted an exponent)
                    event_cancel_with_errno(token_read_ev, EPROTO);
                    free(ctx);
                    return;
                }
            }   break;

            case token_read_state_exponent:
            case token_read_state_exponent_continue:
            {
                if (isdigit(read_character)) {
                    ctx->state = token_read_state_exponent_continue;
                    json_scanner_save_char(scanner, read_character);
                    if (inputstream_ready(scanner->stream))
                        continue;
                    json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
                } else if (ctx->state == token_read_state_exponent) {
                    event_cancel_with_errno(token_read_ev, EPROTO);
                    free(ctx);
                    return;
                } else {
                    json_scanner_ungetc(scanner, read_character);
                    event_return(token_read_ev, (void *)(scanner->last_token = json_token_double));
                    free(ctx);
                    return;
                }
            }   break;

            case token_read_state_string:
            {
                if (read_character == '\\') {
                    ctx->state = token_read_state_string_escaped;
                    json_scanner_save_char(scanner, read_character);
                } else if (read_character == '"') {
                    event_return(token_read_ev, (void *)(scanner->last_token = json_token_string));
                    free(ctx);
                    return;
                } else {
                    // loop in current state
                    json_scanner_save_char(scanner, read_character);
                }
                if (inputstream_ready(scanner->stream))
                    continue;
                json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
            }   break;

            case token_read_state_string_escaped:
            {   // accept the character and transition back to previous state
                ctx->state = token_read_state_string;
                json_scanner_save_char(scanner, read_character);
                if (inputstream_ready(scanner->stream))
                    continue;
                json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
            }   break;

            case token_read_state_true_r:
            case token_read_state_true_u:
            case token_read_state_true_e:
            {
                if (ctx->state == token_read_state_true_r && read_character == 'r') {
                    ctx->state = token_read_state_true_u;
                } else if (ctx->state == token_read_state_true_u && read_character == 'u') {
                    ctx->state = token_read_state_true_e;
                } else if (ctx->state == token_read_state_true_e && read_character == 'e') {
                    json_scanner_save_char(scanner, read_character);
                    event_return(token_read_ev, (void *)(scanner->last_token = json_token_keyword_true));
                    free(ctx);
                    return;
                } else {
                    // unexpected character
                    event_cancel_with_errno(token_read_ev, EPROTO);
                    free(ctx);
                    return;
                }
                json_scanner_save_char(scanner, read_character);
                if (inputstream_ready(scanner->stream))
                    continue;
                json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
            }   break;

            case token_read_state_false_a:
            case token_read_state_false_l:
            case token_read_state_false_s:
            case token_read_state_false_e:
            {
                if (ctx->state == token_read_state_false_a && read_character == 'a') {
                    ctx->state = token_read_state_false_l;
                } else if (ctx->state == token_read_state_false_l && read_character == 'l') {
                    ctx->state = token_read_state_false_s;
                } else if (ctx->state == token_read_state_false_s && read_character == 's') {
                    ctx->state = token_read_state_false_e;
                } else if (ctx->state == token_read_state_false_e && read_character == 'e') {
                    json_scanner_save_char(scanner, read_character);
                    event_return(token_read_ev, (void *)(scanner->last_token = json_token_keyword_false));
                    free(ctx);
                    return;
                } else {
                    // unexpected character
                    event_cancel_with_errno(token_read_ev, EPROTO);
                    free(ctx);
                    return;
                }
                json_scanner_save_char(scanner, read_character);
                if (inputstream_ready(scanner->stream))
                    continue;
                json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
            }   break;

            case token_read_state_null_u:
            case token_read_state_null_l1:
            case token_read_state_null_l2:
            {
                if (ctx->state == token_read_state_null_u && read_character == 'u') {
                    ctx->state = token_read_state_null_l1;
                } else if (ctx->state == token_read_state_null_l1 && read_character == 'l') {
                    ctx->state = token_read_state_null_l2;
                } else if (ctx->state == token_read_state_null_l2 && read_character == 'l') {
                    json_scanner_save_char(scanner, read_character);
                    event_return(token_read_ev, (void *)(scanner->last_token = json_token_keyword_null));
                    free(ctx);
                    return;
                } else {
                    // unexpected character
                    event_cancel_with_errno(token_read_ev, EPROTO);
                    free(ctx);
                    return;
                }
                json_scanner_save_char(scanner, read_character);
                if (inputstream_ready(scanner->stream))
                    continue;
                json_scanner_stream_wait_async(scanner, token_read_ev->loop, json_scanner_stream_ready_cb, ctx);
            }   break;
            }
        } else {
            event_cancel_with_errno(token_read_ev, event_get_errno(ev));
            free(ctx);
        }
        return;
    } while (true);
}

void json_scanner_next_async(json_scanner  *scanner,
                             eventloop     *loop,
                             async_callback callback,
                             void          *user_data)
{
    event *token_read_ev = eventloop_add(loop, callback, user_data);

    struct token_read_ctx *ctx;
    box(struct token_read_ctx, ctx, scanner, token_read_ev,
        token_read_state_skip_spaces, .is_init_state = true);

    json_scanner_stream_wait_async(scanner, loop, json_scanner_stream_ready_cb, ctx);
}

json_token json_scanner_next_finish(const event *ev, int *error)
{
    void *result = NULL;

    if (!event_get_result(ev, &result)) {
        if (error)
            *error = event_get_errno(ev);
        return json_token_error;
    }

    return (json_token)(intptr_t)result;
}

void json_scanner_destroy(json_scanner *scanner)
{
    free(scanner->last_token_buffer);
    scanner->last_token_buffer = NULL;
    inputstream_unref(scanner->stream);
    scanner->stream = NULL;
    free(scanner->filename);
    scanner->filename = NULL;
    if (scanner->message) {
        free(scanner->message);
        scanner->message = NULL;
    }
    free(scanner);
}
