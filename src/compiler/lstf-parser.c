#include "lstf-parser.h"
#include "lstf-variable.h"
#include "compiler/lstf-block.h"
#include "compiler/lstf-elementaccess.h"
#include "compiler/lstf-file.h"
#include "compiler/lstf-patterntest.h"
#include "lstf-array.h"
#include "lstf-object.h"
#include "data-structures/iterator.h"
#include "lstf-expression.h"
#include "lstf-expressionstatement.h"
#include "lstf-parsererror.h"
#include "lstf-methodcall.h"
#include "lstf-report.h"
#include "lstf-sourceloc.h"
#include "lstf-sourceref.h"
#include "lstf-assignment.h"
#include "lstf-variable.h"
#include "lstf-codenode.h"
#include "lstf-scanner.h"
#include "lstf-statement.h"
#include "lstf-memberaccess.h"
#include "lstf-literal.h"
#include "lstf-ellipsis.h"
#include "data-structures/ptr-list.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

lstf_parser *lstf_parser_create(lstf_file *file)
{
    lstf_parser *parser = calloc(1, sizeof *parser);

    parser->scanner = lstf_scanner_create(file);
    parser->file = file;

    return parser;
}

// parser-scanner interfacing
static bool
lstf_parser_accept_token(lstf_parser *parser, lstf_token token)
{
    if (lstf_scanner_current(parser->scanner) == token) {
        lstf_scanner_next(parser->scanner);
        return true;
    }
    return false;
}

static bool
lstf_parser_expect_token(lstf_parser *parser, lstf_token token, lstf_parsererror **error)
{
    if (!lstf_parser_accept_token(parser, token)) {
        lstf_sourceloc after_end = lstf_scanner_get_prev_end_location(parser->scanner);
        if (*after_end.pos) {
            after_end.pos++;
            after_end.column++;
        }
        *error = lstf_parsererror_new(
                &lstf_sourceref_at_location(parser->file, after_end),
                "expected %s", lstf_token_to_string(token));
        return false;
    }
    return true;
}

static lstf_expression *
lstf_parser_parse_simple_name(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);

    if (lstf_scanner_current(parser->scanner) != lstf_token_identifier) {
        *error = lstf_parsererror_new(&lstf_sourceref_at_location(parser->file, begin), 
                "expected %s",
                lstf_token_to_string(lstf_token_identifier));
        return NULL;
    }

    char *member_name = lstf_scanner_get_current_string(parser->scanner);
    lstf_parser_accept_token(parser, lstf_token_identifier);

    return lstf_memberaccess_new(&(lstf_sourceref) {
                parser->file,
                begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            },
            NULL,
            member_name);
}

static lstf_expression *
lstf_parser_parse_member_access_expression(lstf_parser       *parser, 
                                           lstf_sourceloc     begin, 
                                           lstf_expression   *inner, 
                                           lstf_parsererror **error)
{
    if (!lstf_parser_expect_token(parser, lstf_token_period, error))
        return NULL;

    if (lstf_scanner_current(parser->scanner) != lstf_token_identifier) {
        *error = lstf_parsererror_new(&lstf_sourceref_at_location(parser->file, begin), 
                "expected %s for member access",
                lstf_token_to_string(lstf_token_identifier));
        return NULL;
    }

    char *member_name = lstf_scanner_get_current_string(parser->scanner);
    lstf_scanner_next(parser->scanner);

    return lstf_memberaccess_new(&(lstf_sourceref) {
                parser->file,
                begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            },
            inner,
            member_name);
}

static bool
lstf_parser_at_expression(const lstf_parser *parser)
{
    switch (lstf_scanner_current(parser->scanner)) {
    case lstf_token_identifier:
    case lstf_token_openbracket:
    case lstf_token_openbrace:
    case lstf_token_ellipsis:
    case lstf_token_string:
    case lstf_token_keyword_true:
    case lstf_token_keyword_false:
    case lstf_token_keyword_null:
    case lstf_token_double:
    case lstf_token_integer:
        return true;
    default:
        return false;
    }
}

static lstf_expression *
lstf_parser_parse_expression(lstf_parser *parser, lstf_parsererror **error);

static lstf_expression *
lstf_parser_parse_method_call_expression(lstf_parser       *parser, 
                                         lstf_sourceloc     begin,
                                         lstf_expression   *call,
                                         lstf_parsererror **error);

static lstf_expression *
lstf_parser_parse_element_access_expression(lstf_parser       *parser, 
                                            lstf_sourceloc     begin,
                                            lstf_expression   *inner,
                                            lstf_parsererror **error);

/**
 * An expression like, `[1, 2, 3]` or `[1, "hi", null, 3.14159]`, or a pattern
 * like `[1, ..., 3]`
 */
static lstf_expression *
lstf_parser_parse_array_expression(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);

    if (!lstf_parser_expect_token(parser, lstf_token_openbracket, error))
        return NULL;

    ptr_list *elements = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);

    bool is_pattern = false;

    for (lstf_expression *element = NULL; 
            lstf_parser_at_expression(parser) && 
            (element = lstf_parser_parse_expression(parser, error)); ) {
        if (element->expr_type == lstf_expression_type_ellipsis)
            is_pattern = true;
        ptr_list_append(elements, element);
        if (!lstf_parser_accept_token(parser, lstf_token_comma))
            break;
    }

    if (*error || !lstf_parser_expect_token(parser, lstf_token_closebracket, error)) {
        ptr_list_destroy(elements);
        return NULL;
    }

    lstf_expression *array = lstf_array_new(&(lstf_sourceref) {
                parser->file,
                begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            },
            is_pattern);

    for (iterator it = ptr_list_iterator_create(elements); it.has_next; it = iterator_next(it))
        lstf_array_add_element((lstf_array *) array, (lstf_expression *) iterator_get_item(it));

    ptr_list_destroy(elements);
    return array;
}

/**
 * An expression like `{ "prop": 1 }` or `{ prop?: "hello" }` or a pattern like
 * `{ prop?: 3 }`
 */
static lstf_expression *
lstf_parser_parse_object_expression(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);

    if (!lstf_parser_expect_token(parser, lstf_token_openbrace, error))
        return NULL;

    ptr_list *members = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);

    bool is_pattern = false;

    while (true) {
        lstf_sourceloc member_begin = lstf_scanner_get_location(parser->scanner);

        char *member_name = NULL;
        lstf_expression *member_value = NULL;
        bool is_nullable = false;

        switch (lstf_scanner_current(parser->scanner)) {
        case lstf_token_identifier:
        case lstf_token_keyword_true:
        case lstf_token_keyword_false:
        case lstf_token_keyword_const:
        case lstf_token_keyword_let:
        case lstf_token_keyword_for:
        case lstf_token_keyword_of:
        case lstf_token_keyword_null:
        case lstf_token_integer:
        case lstf_token_double:
            member_name = lstf_scanner_get_current_string(parser->scanner);
            lstf_scanner_next(parser->scanner);
            break;
        case lstf_token_string:
            member_name = lstf_scanner_get_current_string(parser->scanner);
            lstf_scanner_next(parser->scanner);
            member_name[strlen(member_name) - 1] = '\0';
            char *temp = strdup(member_name + 1);
            free(member_name);
            member_name = temp;
            break;
        default:
            break;
        }

        if (!member_name)
            break;

        if (lstf_parser_accept_token(parser, lstf_token_questionmark))
            is_nullable = true;

        if (!lstf_parser_expect_token(parser, lstf_token_colon, error)) {
            free(member_name);
            break;
        }

        if (!(member_value = lstf_parser_parse_expression(parser, error))) {
            free(member_name);
            break;
        }

        if (member_value->expr_type == lstf_expression_type_ellipsis)
            is_pattern = true;

        ptr_list_append(members,
                lstf_objectproperty_new(&(lstf_sourceref) {
                        parser->file,
                        member_begin,
                        lstf_scanner_get_prev_end_location(parser->scanner)
                    }, 
                    member_name,
                    is_nullable,
                    member_value));

        free(member_name);

        if (!lstf_parser_accept_token(parser, lstf_token_comma))
            break;
    }

    if (*error || !lstf_parser_expect_token(parser, lstf_token_closebrace, error)) {
        ptr_list_destroy(members);
        return NULL;
    }

    lstf_object *object = lstf_object_new(&(lstf_sourceref) {
                parser->file,
                begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            }, is_pattern);

    for (iterator it = ptr_list_iterator_create(members); it.has_next; it = iterator_next(it))
        lstf_object_add_property(object, iterator_get_item(it));

    return (lstf_expression *) object;
}

/**
 * An expression that is a `...`
 */
static lstf_expression *
lstf_parser_parse_ellipsis_expression(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);

    if (!lstf_parser_expect_token(parser, lstf_token_ellipsis, error))
        return NULL;

    return lstf_ellipsis_new(&(lstf_sourceref) {
                parser->file,
                begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            });
}

static lstf_expression *
lstf_parser_parse_literal_expression(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_expression *expression = NULL;
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);

    switch (lstf_scanner_current(parser->scanner)) {
    case lstf_token_keyword_null:
        lstf_scanner_next(parser->scanner);
        expression = lstf_literal_new(&(lstf_sourceref) {
                    parser->file,
                    begin,
                    lstf_scanner_get_prev_end_location(parser->scanner)
                },
                lstf_literal_type_null,
                (lstf_literal_value) {.string_value = NULL});
        break;
    case lstf_token_integer:
    {
        char *token_string = lstf_scanner_get_current_string(parser->scanner);
        lstf_literal_value literal_value;
        sscanf(token_string, "%"PRId64, &literal_value.integer_value); 
        lstf_scanner_next(parser->scanner);
        expression = lstf_literal_new(&(lstf_sourceref) {
                    parser->file,
                    begin,
                    lstf_scanner_get_prev_end_location(parser->scanner)
                },
                lstf_literal_type_integer,
                literal_value);
        free(token_string);
    }   break;
    case lstf_token_double:
    {
        char *token_string = lstf_scanner_get_current_string(parser->scanner);
        lstf_scanner_next(parser->scanner);
        expression = lstf_literal_new(&(lstf_sourceref) {
                    parser->file,
                    begin,
                    lstf_scanner_get_prev_end_location(parser->scanner)
                },
                lstf_literal_type_integer,
                (lstf_literal_value) {.double_value = strtod(token_string, NULL)});
        free(token_string);
    }   break;
    case lstf_token_keyword_false:
    case lstf_token_keyword_true:
        lstf_scanner_next(parser->scanner);
        expression = lstf_literal_new(&(lstf_sourceref) {
                    parser->file,
                    begin,
                    lstf_scanner_get_prev_end_location(parser->scanner)
                },
                lstf_literal_type_boolean, 
                (lstf_literal_value) {.boolean_value = lstf_scanner_current(parser->scanner) == lstf_token_keyword_true});
        break;
    case lstf_token_string:
        lstf_scanner_next(parser->scanner);
        expression = lstf_literal_new(&(lstf_sourceref) {
                    parser->file,
                    begin,
                    lstf_scanner_get_prev_end_location(parser->scanner)
                },
                lstf_literal_type_string,
                (lstf_literal_value) {.string_value = lstf_scanner_get_current_string(parser->scanner)});
        break;
    default:
        *error = lstf_parsererror_new(&lstf_sourceref_at_location(parser->file, begin), "expected a literal expression");
        break;
    }

    return expression;
}

static lstf_expression *
lstf_parser_parse_await_expression(lstf_parser *parser, lstf_parsererror **error)
{
    if (!lstf_parser_expect_token(parser, lstf_token_keyword_await, error))
        return NULL;

    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);
    lstf_expression *expr = lstf_parser_parse_expression(parser, error);
    if (!expr)
        return NULL;

    if (expr->expr_type == lstf_expression_type_methodcall) {
        ((lstf_methodcall *)expr)->is_awaited = true;
    } else {
        *error = lstf_parsererror_new(
                &(lstf_sourceref){parser->file, begin, lstf_scanner_get_prev_end_location(parser->scanner)},
                "expected method call for %s expression",
                lstf_token_to_string(lstf_token_keyword_await));
        lstf_codenode_unref(expr);
        return NULL;
    }

    return expr;
}

static lstf_expression *
lstf_parser_parse_parenthesized_expression(lstf_parser *parser, lstf_parsererror **error)
{
    if (!lstf_parser_expect_token(parser, lstf_token_openparen, error))
        return NULL;

    lstf_expression *inner = lstf_parser_parse_expression(parser, error);
    if (!inner)
        return NULL;

    if (!lstf_parser_expect_token(parser, lstf_token_closeparen, error)) {
        lstf_codenode_unref(inner);
        return NULL;
    }

    return inner;
}

static lstf_expression *
lstf_parser_parse_expression(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_expression *expression = NULL;
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);

    switch (lstf_scanner_current(parser->scanner)) {
    case lstf_token_identifier:
        expression = lstf_parser_parse_simple_name(parser, error);
        break;
    case lstf_token_openbracket:
        expression = lstf_parser_parse_array_expression(parser, error);
        break;
    case lstf_token_openbrace:
        expression = lstf_parser_parse_object_expression(parser, error);
        break;
    case lstf_token_ellipsis:
        return lstf_parser_parse_ellipsis_expression(parser, error);
    case lstf_token_string:
    case lstf_token_keyword_true:
    case lstf_token_keyword_false:
    case lstf_token_keyword_null:
    case lstf_token_double:
    case lstf_token_integer:
        expression = lstf_parser_parse_literal_expression(parser, error);
        break;
    case lstf_token_keyword_await:
        expression = lstf_parser_parse_await_expression(parser, error);
        break;
    case lstf_token_openparen:
        expression = lstf_parser_parse_parenthesized_expression(parser, error);
        break;
    default:
        *error = lstf_parsererror_new(
                &lstf_sourceref_at_location(parser->file, begin),
                "expected expression, got %s", lstf_token_to_string(lstf_scanner_current(parser->scanner)));
        break;
    }

    for (lstf_expression *next_expr = expression; next_expr && !*error; ) {
        expression = next_expr;
        next_expr = NULL;
        switch (lstf_scanner_current(parser->scanner)) {
        case lstf_token_period:
            next_expr = lstf_parser_parse_member_access_expression(parser, begin, expression, error);
            break;
        case lstf_token_openparen:
            next_expr = lstf_parser_parse_method_call_expression(parser, begin, expression, error);
            break;
        case lstf_token_openbracket:
            next_expr = lstf_parser_parse_element_access_expression(parser, begin, expression, error);
            break;
        default:
            break;
        }
    }

    if (*error) {
        if (expression)
            lstf_codenode_unref(expression);
        return NULL;
    }

    return expression;
}

static lstf_statement *
lstf_parser_parse_assignment_statement(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);
    bool is_declaration = lstf_parser_accept_token(parser, lstf_token_keyword_let);

    char *variable_name = lstf_scanner_get_current_string(parser->scanner);

    if (!lstf_parser_expect_token(parser, lstf_token_identifier, error)) {
        free(variable_name);
        return NULL;
    }

    lstf_variable *variable = lstf_variable_new(&(lstf_sourceref) {
                parser->file,
                begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            }, variable_name);

    if (!lstf_parser_expect_token(parser, lstf_token_assignment, error)) {
        lstf_codenode_unref(variable);
        return NULL;
    }

    lstf_expression *expression = lstf_parser_parse_expression(parser, error);
    if (!expression) {
        lstf_codenode_unref(variable);
        return NULL;
    }

    if (!lstf_parser_expect_token(parser, lstf_token_semicolon, error)) {
        lstf_codenode_unref(variable);
        lstf_codenode_unref(expression);
        return NULL;
    }

    return lstf_assignment_new(
            &(lstf_sourceref) { parser->file, begin, lstf_scanner_get_prev_end_location(parser->scanner) }, 
            is_declaration, 
            variable,
            expression);
}

static lstf_expression *
lstf_parser_parse_method_call_expression(lstf_parser       *parser, 
                                         lstf_sourceloc     begin,
                                         lstf_expression   *call,
                                         lstf_parsererror **error)
{

    if (!lstf_parser_expect_token(parser, lstf_token_openparen, error))
        return NULL;

    // parse arguments
    ptr_list *arguments = ptr_list_new((collection_item_ref_func) lstf_codenode_ref, 
            (collection_item_unref_func) lstf_codenode_unref);
    lstf_expression *arg = NULL;

    if (lstf_scanner_current(parser->scanner) != lstf_token_closeparen) {
        while ((arg = lstf_parser_parse_expression(parser, error))) {
            ptr_list_append(arguments, arg);
            if (!lstf_parser_accept_token(parser, lstf_token_comma))
                break;
        }
    }

    if (*error || !lstf_parser_expect_token(parser, lstf_token_closeparen, error)) {
        ptr_list_destroy(arguments);
        return NULL;
    }

    return lstf_methodcall_new(
            &(lstf_sourceref) {parser->file, begin, lstf_scanner_get_prev_end_location(parser->scanner)},
            call,
            arguments);   
}

static lstf_expression *
lstf_parser_parse_element_access_expression(lstf_parser       *parser, 
                                            lstf_sourceloc     begin,
                                            lstf_expression   *inner,
                                            lstf_parsererror **error)
{
    if (!lstf_parser_expect_token(parser, lstf_token_openbracket, error))
        return NULL;

    ptr_list *arguments = ptr_list_new((collection_item_ref_func) lstf_codenode_ref, 
            (collection_item_unref_func) lstf_codenode_unref);
    lstf_expression *arg = NULL;

    if (lstf_scanner_current(parser->scanner) != lstf_token_closebracket) {
        while ((arg = lstf_parser_parse_expression(parser, error))) {
            ptr_list_append(arguments, arg);
            if (!lstf_parser_accept_token(parser, lstf_token_comma))
                break;
        }
    }

    if (*error || !lstf_parser_expect_token(parser, lstf_token_closebracket, error)) {
        ptr_list_destroy(arguments);
        return NULL;
    }

    return lstf_elementaccess_new(
            &(lstf_sourceref) {parser->file, begin, lstf_scanner_get_prev_end_location(parser->scanner)},
            inner, 
            arguments);
}

static lstf_expression *
lstf_parser_parse_statement_expression(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_expression *expression = NULL;
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);

    switch (lstf_scanner_current(parser->scanner)) {
    case lstf_token_identifier:
        expression = lstf_parser_parse_simple_name(parser, error);
        break;
    case lstf_token_string:
    case lstf_token_keyword_true:
    case lstf_token_keyword_false:
    case lstf_token_keyword_null:
    case lstf_token_double:
    case lstf_token_integer:
        expression = lstf_parser_parse_literal_expression(parser, error);
        break;
    case lstf_token_keyword_await:
        expression = lstf_parser_parse_await_expression(parser, error);
        break;
    case lstf_token_openparen:
        expression = lstf_parser_parse_parenthesized_expression(parser, error);
        break;
    default:
        *error = lstf_parsererror_new(
                &lstf_sourceref_at_location(parser->file, begin),
                "expected statement, got %s", lstf_token_to_string(lstf_scanner_current(parser->scanner)));
        break;
    }

    for (lstf_expression *next_expr = expression; next_expr && !*error; ) {
        expression = next_expr;
        next_expr = NULL;
        switch (lstf_scanner_current(parser->scanner)) {
        case lstf_token_period:
            next_expr = lstf_parser_parse_member_access_expression(parser, begin, expression, error);
            break;
        case lstf_token_openparen:
            next_expr = lstf_parser_parse_method_call_expression(parser, begin, expression, error);
            break;
        case lstf_token_openbracket:
            next_expr = lstf_parser_parse_element_access_expression(parser, begin, expression, error);
            break;
        default:
            break;
        }
    }

    if (!expression || *error) {
        if (expression)
            lstf_codenode_unref(expression);
        return NULL;
    }

    if (expression->expr_type != lstf_expression_type_methodcall) {
        *error = lstf_parsererror_new(&expression->parent_struct.source_reference,
                "%s is not valid for a statement", lstf_expression_type_to_string(expression->expr_type));
        lstf_codenode_unref(expression);
        return NULL;
    }

    return expression;
}

static lstf_statement *
lstf_parser_parse_expression_statement(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);

    lstf_expression *expression = lstf_parser_parse_statement_expression(parser, error);
    if (!expression)
        return NULL;

    if (!lstf_parser_expect_token(parser, lstf_token_semicolon, error)) {
        lstf_codenode_unref(expression);
        return NULL;
    }

    return lstf_expressionstatement_new(
            &lstf_sourceref_at_location(parser->file, begin), 
            expression);
}

static lstf_statement *
lstf_parser_parse_pattern_test_statement(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);

    lstf_expression *pattern = lstf_parser_parse_expression(parser, error);
    if (!pattern)
        return NULL;

    if (!lstf_parser_expect_token(parser, lstf_token_equivalent, error)) {
        lstf_codenode_unref(pattern);
        return NULL;
    }

    lstf_expression *rhs = lstf_parser_parse_expression(parser, error);
    if (!rhs) {
        lstf_codenode_unref(pattern);
        return NULL;
    }

    if (!lstf_parser_expect_token(parser, lstf_token_semicolon, error)) {
        lstf_codenode_unref(pattern);
        lstf_codenode_unref(rhs);
        return NULL;
    }

    return lstf_patterntest_new(
            &(lstf_sourceref) {parser->file, begin, lstf_scanner_get_prev_end_location(parser->scanner)}, 
            pattern,
            rhs);
}

static lstf_statement *
lstf_parser_parse_statement(lstf_parser *parser, lstf_parsererror **error)
{
    switch (lstf_scanner_current(parser->scanner)) {
    case lstf_token_keyword_let:
        return lstf_parser_parse_assignment_statement(parser, error);
    case lstf_token_identifier:
    {
        lstf_token next_token = lstf_scanner_peek_next(parser->scanner);

        if (next_token == lstf_token_eof) {
            *error = lstf_parsererror_new(
                    &lstf_sourceref_at_location(parser->file, 
                        lstf_scanner_get_location(parser->scanner)), 
                    "expected statement, got %s", lstf_token_to_string(next_token)); 
            return NULL;
        }

        if (next_token == lstf_token_assignment)
            return lstf_parser_parse_assignment_statement(parser, error);
        else if (next_token == lstf_token_equivalent)
            return lstf_parser_parse_pattern_test_statement(parser, error);
        return lstf_parser_parse_expression_statement(parser, error);
    } break;
    case lstf_token_openbracket:
    case lstf_token_openbrace:
    case lstf_token_integer:
    case lstf_token_double:
    case lstf_token_keyword_true:
    case lstf_token_keyword_false:
    case lstf_token_keyword_null:
        return lstf_parser_parse_pattern_test_statement(parser, error);
    case lstf_token_keyword_await:
        return lstf_parser_parse_expression_statement(parser, error);
    default:
        *error = lstf_parsererror_new(
                &lstf_sourceref_at_location(parser->file,
                    lstf_scanner_get_location(parser->scanner)),
                "expected statement, got %s", 
                lstf_token_to_string(lstf_scanner_current(parser->scanner)));
        break;
    }

    return NULL;
}

/**
 * Returns a list of `(lstf_statement *)` objects
 */
static ptr_list *
lstf_parser_parse_statement_list(lstf_parser *parser)
{
    ptr_list *statements = ptr_list_new((collection_item_ref_func) lstf_codenode_ref, 
            (collection_item_unref_func) lstf_codenode_unref);

    lstf_statement *stmt = NULL;
    lstf_parsererror *error = NULL;

    while (lstf_scanner_current(parser->scanner) != lstf_token_eof && 
            ((stmt = lstf_parser_parse_statement(parser, &error)) || error)) {
        if (stmt) {
            ptr_list_append(statements, stmt);
        } else {
            // skip over bad statement and continue
            lstf_report_error(&error->source_reference, "%s", error->message);
            parser->num_errors++;
            lstf_parsererror_destroy(error);
            error = NULL;

            // attempt to recover
            bool possible_stmt_start = false;
            while (!possible_stmt_start && lstf_scanner_current(parser->scanner) != lstf_token_eof) {
                switch (lstf_scanner_current(parser->scanner)) {
                case lstf_token_keyword_let:
                case lstf_token_keyword_await:
                // currently unsupported:
                case lstf_token_keyword_for:
                case lstf_token_keyword_const:
                    possible_stmt_start = true;
                    break;
                case lstf_token_identifier:
                    possible_stmt_start = lstf_scanner_peek_next(parser->scanner) == lstf_token_assignment ||
                        lstf_scanner_peek_next(parser->scanner) == lstf_token_equivalent;
                    break;
                case lstf_token_semicolon:
                    possible_stmt_start = true;
                    lstf_scanner_next(parser->scanner);
                    break;
                default:
                    break;
                }

                if (!possible_stmt_start)
                    lstf_scanner_next(parser->scanner);
            }
        }
    }

    return statements;
}

static void
lstf_parser_parse_main_block(lstf_parser *parser)
{
    lstf_block_clear_statements(parser->file->main_block);

    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);
    ptr_list *statements = lstf_parser_parse_statement_list(parser);

    for (iterator it = ptr_list_iterator_create(statements); it.has_next; it = iterator_next(it))
        lstf_block_add_statement(parser->file->main_block, iterator_get_item(it));

    ((lstf_codenode *)parser->file->main_block)->source_reference = (lstf_sourceref) {
        parser->file,
        begin,
        lstf_scanner_get_prev_end_location(parser->scanner)
    };

    ptr_list_destroy(statements);
}

void lstf_parser_parse(lstf_parser *parser)
{
    if (parser->scanner->num_errors == 0)
        lstf_parser_parse_main_block(parser);
}

void lstf_parser_destroy(lstf_parser *parser)
{
    lstf_scanner_destroy(parser->scanner);
    parser->scanner = NULL;
    free(parser);
}
