#include "json-scanner.h"
#include "compiler/lstf-sourceloc.h"
#include "data-structures/string-builder.h"
#include "json/json-parser.h"
#include <stdarg.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *json_token_to_string(json_token token)
{
    switch (token) {
    case json_token_eof:
        return "EOF";
    case json_token_error:
        return NULL;
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
    }

    fprintf(stderr, "%s: unexpected value %d for json_token\n", __func__, token);
    abort();
}

#if (_WIN32 || _WIN64)
#include <windows.h>
#include <io.h>
static char *get_filename_from_fd(int fd) {
    HANDLE fh = (HANDLE) _get_osfhandle(fd);
    char resolved_path[MAX_PATH] = { '\0' };
    DWORD ret = 0;

    if (!fh) {
        fprintf(stderr, "%s: could not get OS f-handle from fd %d\n", __func__, fd);
        return NULL;
    }

    if (!(ret = GetFinalPathNameByHandle(fh, resolved_path, sizeof resolved_path, 0)))
        return NULL;

    return _strdup(resolved_path);
}
#else
#include <unistd.h>
#include <limits.h>
static char *get_filename_from_fd(int fd)
{
    string *path_sb = string_new();
    char resolved_path[PATH_MAX] = { '\0' };
    ssize_t ret = 0;

    string_appendf(path_sb, "/proc/self/fd/%d", fd); 
    ret = readlink(path_sb->buffer, resolved_path, sizeof resolved_path);
    free(string_destroy(path_sb));

    if (ret != -1)
        return strdup(resolved_path);
    return NULL;
}
#endif

json_scanner *json_scanner_create_from_stream(FILE *stream, bool close_stream)
{
    json_scanner *scanner = NULL;

    if (!stream)
        return NULL;

    if (!(scanner = calloc(1, sizeof *scanner))) {
        perror("could not create JSON scanner");
        exit(EXIT_FAILURE);
    }

    scanner->stream = stream;
    if (scanner->stream == stdin)
        scanner->filename = strdup("<stdin>");
    else if (!(scanner->filename = get_filename_from_fd(fileno(scanner->stream)))){
        string *sb = string_new();
        string_appendf(sb, "<fd %d>", fileno(scanner->stream));
        scanner->filename = string_destroy(sb);
    }
    scanner->close_stream = close_stream;
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

static char json_scanner_getc(json_scanner *scanner)
{
    char read_character = fgetc(scanner->stream);

    scanner->prev_char_source_location = scanner->source_location;
    if (read_character == '\n' || read_character == '\r') {
        scanner->source_location.line++;
        scanner->source_location.column = 0;
    } else {
        scanner->source_location.column++;
    }

    return read_character;
}

static void json_scanner_ungetc(json_scanner *scanner, char unread_character)
{
    if (ungetc(unread_character, scanner->stream) != EOF)
        scanner->source_location = scanner->prev_char_source_location;
}


__attribute__ ((format (printf, 3, 4)))
static void 
json_scanner_report_message(json_scanner *scanner, lstf_sourceloc source_location, const char *format, ...)
{
    string *sb = string_new();
    string_appendf(sb, "%s:%d:%d: ",
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
    if (feof(scanner->stream) || ferror(scanner->stream))
        return scanner->last_token = json_token_eof;

    if (scanner->message) {
        free(scanner->message);
        scanner->message = NULL;
    }

    scanner->last_token_length = 0;
    if (scanner->last_token_buffer)
        scanner->last_token_buffer[scanner->last_token_length] = '\0';

    char current_char = json_scanner_getc(scanner);
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
        lstf_sourceloc begin_sourceloc = scanner->source_location;
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
    case EOF:
        return scanner->last_token = json_token_eof;
    default:
    {
        string *sb = string_new();
        string_appendf(sb, "%s:%d:%d: error: unexpected character `%c'",
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

const char *json_scanner_get_message(json_scanner *scanner)
{
    return scanner->message;
}

void json_scanner_destroy(json_scanner *scanner)
{
    free(scanner->last_token_buffer);
    scanner->last_token_buffer = NULL;
    if (scanner->close_stream)
        fclose(scanner->stream);
    scanner->stream = NULL;
    free(scanner->filename);
    scanner->filename = NULL;
    if (scanner->message) {
        free(scanner->message);
        scanner->message = NULL;
    }
    free(scanner);
}
