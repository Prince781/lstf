#include "lstf-parser.h"
#include "lstf-variable.h"
#include "compiler/lstf-returnstatement.h"
#include "lstf-nulltype.h"
#include "lstf-arraytype.h"
#include "lstf-functiontype.h"
#include "lstf-interfacetype.h"
#include "lstf-symbol.h"
#include "lstf-typealias.h"
#include "lstf-datatype.h"
#include "lstf-uniontype.h"
#include "lstf-function.h"
#include "lstf-constant.h"
#include "lstf-declaration.h"
#include "lstf-enum.h"
#include "lstf-interface.h"
#include "lstf-block.h"
#include "lstf-elementaccess.h"
#include "lstf-file.h"
#include "lstf-patterntest.h"
#include "lstf-unresolvedtype.h"
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

static char *
lstf_parser_parse_identifier(lstf_parser *parser, lstf_parsererror **error)
{
    char *identifier = lstf_scanner_get_current_string(parser->scanner);

    if (!lstf_parser_expect_token(parser, lstf_token_identifier, error)) {
        free(identifier);
        return NULL;
    }

    return identifier;
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

    lstf_expression *expr = lstf_memberaccess_new(&(lstf_sourceref) {
                parser->file,
                begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            },
            NULL,
            member_name);
    free(member_name);
    return expr;
}

static lstf_expression *
lstf_parser_parse_member_access_expression(lstf_parser       *parser, 
                                           lstf_expression   *inner, 
                                           lstf_parsererror **error)
{
    if (!lstf_parser_expect_token(parser, lstf_token_period, error))
        return NULL;

    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);
    if (lstf_scanner_current(parser->scanner) != lstf_token_identifier) {
        *error = lstf_parsererror_new(&lstf_sourceref_at_location(parser->file, begin), 
                "expected %s for member access",
                lstf_token_to_string(lstf_token_identifier));
        return NULL;
    }

    char *member_name = lstf_scanner_get_current_string(parser->scanner);
    lstf_scanner_next(parser->scanner);

    lstf_expression *expr = lstf_memberaccess_new(&(lstf_sourceref) {
                parser->file,
                begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            },
            inner,
            member_name);
    free(member_name);
    return expr;
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

static lstf_datatype *
lstf_parser_parse_simple_data_type(lstf_parser *parser, lstf_parsererror **error);

static lstf_datatype *
lstf_parser_parse_data_type(lstf_parser *parser, lstf_parsererror **error);

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
    ptr_list_destroy(members);

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
                lstf_literal_type_double,
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
            next_expr = lstf_parser_parse_member_access_expression(parser, expression, error);
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
    lstf_sourceloc var_begin = lstf_scanner_get_location(parser->scanner);

    char *variable_name = lstf_scanner_get_current_string(parser->scanner);

    if (!lstf_parser_expect_token(parser, lstf_token_identifier, error)) {
        free(variable_name);
        return NULL;
    }

    lstf_expression *lhs = lstf_memberaccess_new(&(lstf_sourceref) {
                parser->file,
                var_begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            }, NULL, variable_name);
    free(variable_name);
    variable_name = NULL;

    if (!is_declaration) {
        for (lstf_expression *next_expr = lhs; next_expr; ) {
            lhs = next_expr;
            next_expr = NULL;

            switch (lstf_scanner_current(parser->scanner)) {
            case lstf_token_period:
                next_expr = lstf_parser_parse_member_access_expression(parser, lhs, error);
                break;
            case lstf_token_openbracket:
                next_expr = lstf_parser_parse_element_access_expression(parser, var_begin, lhs, error);
                break;
            default:
                break;
            }
        }
    } else {
        if (lstf_parser_accept_token(parser, lstf_token_colon)) {
            lstf_datatype *variable_type = lstf_parser_parse_data_type(parser, error);
            if (variable_type)
                lstf_expression_set_value_type(lhs, variable_type);
        }
    }

    if (*error) {
        lstf_codenode_unref(lhs);
        return NULL;
    }

    if (!lstf_parser_expect_token(parser, lstf_token_assignment, error)) {
        lstf_codenode_unref(lhs);
        return NULL;
    }

    lstf_expression *expression = lstf_parser_parse_expression(parser, error);
    if (!expression) {
        lstf_codenode_unref(lhs);
        return NULL;
    }

    if (!lstf_parser_expect_token(parser, lstf_token_semicolon, error)) {
        lstf_codenode_unref(lhs);
        lstf_codenode_unref(expression);
        return NULL;
    }

    return lstf_assignment_new(
            &(lstf_sourceref) { parser->file, begin, lstf_scanner_get_prev_end_location(parser->scanner) }, 
            is_declaration, 
            lhs,
            expression);
}

static lstf_statement *
lstf_parser_parse_enum_declaration(lstf_parser *parser, lstf_parsererror **error)
{
    if (!lstf_parser_expect_token(parser, lstf_token_keyword_enum, error))
        return NULL;

    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);
    char *symbol_name = lstf_parser_parse_identifier(parser, error);
    if (!symbol_name)
        return NULL;
    lstf_sourceloc end = lstf_scanner_get_prev_end_location(parser->scanner);

    ptr_list *enum_members = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);

    if (!lstf_parser_expect_token(parser, lstf_token_openbrace, error)) {
        free(symbol_name);
        ptr_list_destroy(enum_members);
        return NULL;
    }

    while (true) {
        lstf_sourceloc member_begin = lstf_scanner_get_location(parser->scanner);
        char *member_name = lstf_parser_parse_identifier(parser, error);
        if (!member_name)
            break;

        if (!lstf_parser_expect_token(parser, lstf_token_assignment, error)) {
            free(member_name);
            break;
        }

        lstf_expression *member_value = lstf_parser_parse_expression(parser, error);
        if (!member_value) {
            free(member_name);
            break;
        }

        lstf_symbol *constant = lstf_constant_new(&(lstf_sourceref) {
                    parser->file,
                    member_begin,
                    lstf_scanner_get_prev_end_location(parser->scanner)
                }, member_name, member_value);

        ptr_list_append(enum_members, constant);
        free(member_name);

        if (!lstf_parser_accept_token(parser, lstf_token_comma))
            break;
    }

    if (*error) {
        free(symbol_name);
        ptr_list_destroy(enum_members);
        return NULL;
    }

    if (ptr_list_is_empty(enum_members)) {
        *error = lstf_parsererror_new(&(lstf_sourceref) {
                    parser->file,
                    begin,
                    lstf_scanner_get_prev_end_location(parser->scanner)
                }, "enum must contain at least one member");
        free(symbol_name);
        ptr_list_destroy(enum_members);
        return NULL;
    }

    if (!lstf_parser_expect_token(parser, lstf_token_closebrace, error)) {
        free(symbol_name);
        ptr_list_destroy(enum_members);
        return NULL;
    }

    lstf_enum *enum_symbol =
        (lstf_enum *) lstf_enum_new(&(lstf_sourceref) {
                    parser->file,
                    begin,
                    end
                }, symbol_name, false);

    for (iterator it = ptr_list_iterator_create(enum_members); it.has_next; it = iterator_next(it)) {
        lstf_constant *constant = iterator_get_item(it);
        lstf_enum_add_member(enum_symbol, constant);
    }

    free(symbol_name);
    ptr_list_destroy(enum_members);
    return lstf_declaration_new_from_enum(&lstf_codenode_cast(enum_symbol)->source_reference, enum_symbol);
}

static lstf_datatype *
lstf_parser_parse_object_data_type(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);

    if (!lstf_parser_expect_token(parser, lstf_token_openbrace, error))
        return NULL;

    ptr_list *properties_list = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);
    while (lstf_scanner_current(parser->scanner) == lstf_token_identifier) {
        lstf_sourceloc property_begin = lstf_scanner_get_location(parser->scanner);
        char *property_name = lstf_parser_parse_identifier(parser, error);
        if (!property_name)
            break;
        bool is_optional = lstf_parser_accept_token(parser, lstf_token_questionmark);
        if (!lstf_parser_expect_token(parser, lstf_token_colon, error)) {
            free(property_name);
            break;
        }
        lstf_datatype *property_type = lstf_parser_parse_data_type(parser, error);
        if (!property_type) {
            free(property_name);
            break;
        }

        ptr_list_append(properties_list, lstf_interfaceproperty_new(&(lstf_sourceref) {
                        parser->file,
                        property_begin,
                        lstf_scanner_get_prev_end_location(parser->scanner)
                    }, property_name, is_optional, property_type, false));

        if (!lstf_parser_accept_token(parser, lstf_token_semicolon))
            break;
    }

    if (*error || !lstf_parser_expect_token(parser, lstf_token_closebrace, error)) {
        ptr_list_destroy(properties_list);
        return NULL;
    }

    lstf_interface *anonymous_interface =
        lstf_interface_new(&(lstf_sourceref) {
                    parser->file,
                    begin,
                    lstf_scanner_get_prev_end_location(parser->scanner)
                }, NULL, true, false);

    for (iterator it = ptr_list_iterator_create(properties_list); it.has_next; it = iterator_next(it))
        lstf_interface_add_member(anonymous_interface, (lstf_interfaceproperty *)iterator_get_item(it));

    ptr_list_destroy(properties_list);
    return lstf_interfacetype_new(&(lstf_sourceref) {
                parser->file,
                begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            }, anonymous_interface);
}

static lstf_datatype *
lstf_parser_parse_function_data_type(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);
    bool is_async = lstf_parser_accept_token(parser, lstf_token_keyword_async);

    if (!lstf_parser_expect_token(parser, lstf_token_openparen, error))
        return NULL;

    ptr_list *parameters_list = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);
    if (lstf_scanner_current(parser->scanner) == lstf_token_identifier) {
        while (true) {
            lstf_sourceloc parameter_begin = lstf_scanner_get_location(parser->scanner);
            char *parameter_name = lstf_parser_parse_identifier(parser, error);
            if (!parameter_name)
                break;

            if (!lstf_parser_accept_token(parser, lstf_token_colon)) {
                *error = lstf_parsererror_new(&lstf_sourceref_at_location(parser->file,
                            lstf_scanner_get_location(parser->scanner)),
                        "expected %s, then data type here",
                        lstf_token_to_string(lstf_token_colon));
                free(parameter_name);
                break;
            }

            lstf_datatype *parameter_type = lstf_parser_parse_data_type(parser, error);
            if (!parameter_type) {
                free(parameter_name);
                break;
            }

            lstf_variable *parameter = (lstf_variable *)
                lstf_variable_new(&(lstf_sourceref) {
                            parser->file,
                            parameter_begin,
                            lstf_scanner_get_prev_end_location(parser->scanner)
                        }, parameter_name, false);
            lstf_variable_set_variable_type(parameter, parameter_type);
            ptr_list_append(parameters_list, parameter);
            free(parameter_name);

            if (!lstf_parser_accept_token(parser, lstf_token_comma))
                break;
        }
    }

    if (!lstf_parser_expect_token(parser, lstf_token_closeparen, error) ||
            !lstf_parser_expect_token(parser, lstf_token_doublerightarrow, error)) {
        ptr_list_destroy(parameters_list);
        return NULL;
    }

    lstf_datatype *return_type = lstf_parser_parse_data_type(parser, error);
    if (!return_type) {
        ptr_list_destroy(parameters_list);
        return NULL;
    }

    lstf_functiontype *function_type = (lstf_functiontype *)
        lstf_functiontype_new(&(lstf_sourceref) {
                parser->file,
                begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            }, return_type, is_async);

    for (iterator it = ptr_list_iterator_create(parameters_list); it.has_next; it = iterator_next(it)) {
        lstf_variable *parameter = iterator_get_item(it);
        lstf_functiontype_add_parameter(function_type,
                lstf_symbol_cast(parameter)->name, parameter->variable_type);
    }

    ptr_list_destroy(parameters_list);
    return (lstf_datatype *) function_type;
}

static lstf_datatype *
lstf_parser_parse_element_data_type(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);

    switch (lstf_scanner_current(parser->scanner)) {
    case lstf_token_keyword_null:
        if (!lstf_parser_expect_token(parser, lstf_token_keyword_null, error))
            return NULL;
        return lstf_nulltype_new(&(lstf_sourceref) {
                    parser->file,
                    begin,
                    lstf_scanner_get_prev_end_location(parser->scanner)
                });
        break;
    case lstf_token_identifier:
    {
        char *type_name = lstf_parser_parse_identifier(parser, error);
        if (type_name) {
            lstf_datatype *data_type = lstf_unresolvedtype_new(&(lstf_sourceref) {
                        parser->file,
                        begin,
                        lstf_scanner_get_prev_end_location(parser->scanner)
                    }, type_name);
            free(type_name);
            return data_type;
        }
    } break;
    case lstf_token_openparen:
    {
        if (!lstf_parser_expect_token(parser, lstf_token_openparen, error))
            return NULL;
        lstf_datatype *data_type = lstf_parser_parse_data_type(parser, error);
        if (!data_type || !lstf_parser_expect_token(parser, lstf_token_closeparen, error)) {
            lstf_codenode_unref(data_type);
            return NULL;
        }
        return data_type;
    } break;
    case lstf_token_openbrace:
        return lstf_parser_parse_object_data_type(parser, error);
    default:
        *error = lstf_parsererror_new(&lstf_sourceref_at_location(parser->file,
                    lstf_scanner_get_location(parser->scanner)),
                "expected data type here");
        break;
    }

    return NULL;
}

static lstf_datatype *
lstf_parser_parse_simple_data_type(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);
    lstf_datatype *data_type = NULL;

    switch (lstf_scanner_current(parser->scanner)) {
    case lstf_token_keyword_null:
    case lstf_token_identifier:
    case lstf_token_openbrace:
        data_type = lstf_parser_parse_element_data_type(parser, error);
        break;
    case lstf_token_openparen:
    {
        const unsigned saved_token_idx = parser->scanner->current_token_idx;
        lstf_scanner_next(parser->scanner);
        if (lstf_parser_accept_token(parser, lstf_token_keyword_async) ||
                (lstf_parser_accept_token(parser, lstf_token_identifier) &&
                 (lstf_parser_accept_token(parser, lstf_token_questionmark) || 
                  lstf_parser_accept_token(parser, lstf_token_colon)))) {
            lstf_scanner_rewind(parser->scanner, saved_token_idx);
            return lstf_parser_parse_function_data_type(parser, error);
        }
        lstf_scanner_rewind(parser->scanner, saved_token_idx);
        data_type = lstf_parser_parse_element_data_type(parser, error);
    } break;
    default:
        *error = lstf_parsererror_new(&lstf_sourceref_at_location(parser->file, begin),
                "expected data type here");
        break;
    }

    if (*error) {
        lstf_codenode_unref(data_type);
        return NULL;
    }

    while (lstf_parser_accept_token(parser, lstf_token_openbracket)) {
        if (!lstf_parser_expect_token(parser, lstf_token_closebracket, error))
            break;
        data_type = lstf_arraytype_new(&(lstf_sourceref) {
                    parser->file,
                    begin,
                    lstf_scanner_get_prev_end_location(parser->scanner)
                }, data_type);
    }

    if (*error) {
        lstf_codenode_unref(data_type);
        return NULL;
    }

    return data_type;
}

static lstf_datatype *
lstf_parser_parse_data_type(lstf_parser *parser, lstf_parsererror **error)
{
    ptr_list *unioned_data_types = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);

    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);
    do {
        lstf_datatype *simple_dt = lstf_parser_parse_simple_data_type(parser, error);
        if (!simple_dt)
            break;
        ptr_list_append(unioned_data_types, simple_dt);
    } while (lstf_parser_accept_token(parser, lstf_token_verticalbar));

    if (*error) {
        ptr_list_destroy(unioned_data_types);
        return NULL;
    }

    lstf_datatype *new_data_type = NULL;
    if (unioned_data_types->length == 1)
        new_data_type = lstf_datatype_copy(iterator_get_item(ptr_list_iterator_create(unioned_data_types)));
    else {
        iterator it = ptr_list_iterator_create(unioned_data_types);
        new_data_type = lstf_uniontype_new(&(lstf_sourceref) {
                    parser->file,
                    begin,
                    lstf_scanner_get_prev_end_location(parser->scanner)
                }, iterator_get_item(it), NULL);
        it = iterator_next(it);
        while (it.has_next) {
            lstf_uniontype_add_option(lstf_uniontype_cast(new_data_type), iterator_get_item(it));
            it = iterator_next(it);
        }
    }

    ptr_list_destroy(unioned_data_types);
    return new_data_type;
}

static ptr_list *
lstf_parser_parse_statement_list(lstf_parser *parser, bool in_root_scope);

static lstf_statement *
lstf_parser_parse_function_declaration(lstf_parser *parser, bool in_class_declaration, lstf_parsererror **error)
{
    bool is_async = lstf_parser_accept_token(parser, lstf_token_keyword_async);

    if (!lstf_parser_expect_token(parser, lstf_token_keyword_fun, error))
        return NULL;

    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);
    char *function_name = lstf_parser_parse_identifier(parser, error);
    if (!function_name)
        return NULL;
    lstf_sourceloc end = lstf_scanner_get_prev_end_location(parser->scanner);

    if (!lstf_parser_expect_token(parser, lstf_token_openparen, error)) {
        free(function_name);
        return NULL;
    }

    // parse parameters
    ptr_list *parameters = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);

    if (lstf_scanner_current(parser->scanner) == lstf_token_identifier) {
        while (true) {
            lstf_sourceloc parameter_begin = lstf_scanner_get_location(parser->scanner);
            char *parameter_name = lstf_parser_parse_identifier(parser, error);
            if (!parameter_name)
                break;

            if (!lstf_parser_accept_token(parser, lstf_token_colon)) {
                *error = lstf_parsererror_new(&lstf_sourceref_at_location(parser->file, 
                            lstf_scanner_get_location(parser->scanner)),
                        "expected %s, then data type here",
                        lstf_token_to_string(lstf_token_colon));
                free(parameter_name);
                break;
            }

            lstf_datatype *parameter_type = lstf_parser_parse_data_type(parser, error);
            if (!parameter_type) {
                free(parameter_name);
                break;
            }

            lstf_variable *parameter = (lstf_variable *)
                lstf_variable_new(&(lstf_sourceref) {
                        parser->file,
                        parameter_begin,
                        lstf_scanner_get_prev_end_location(parser->scanner)
                    }, parameter_name, false);
            lstf_variable_set_variable_type(parameter, parameter_type);
            free(parameter_name);

            ptr_list_append(parameters, parameter);

            if (!lstf_parser_accept_token(parser, lstf_token_comma))
                break;
        }
    }

    if (*error || !lstf_parser_expect_token(parser, lstf_token_closeparen, error)) {
        free(function_name);
        ptr_list_destroy(parameters);
        return NULL;
    }

    // parse return type
    if (!lstf_parser_accept_token(parser, lstf_token_colon)) {
        *error = lstf_parsererror_new(&lstf_sourceref_at_location(parser->file, 
                    lstf_scanner_get_location(parser->scanner)),
                "expected return type here (expected %s, then data type)",
                lstf_token_to_string(lstf_token_colon));
        free(function_name);
        ptr_list_destroy(parameters);
        return NULL;
    }

    lstf_datatype *return_type = lstf_parser_parse_data_type(parser, error);
    if (!return_type) {
        free(function_name);
        ptr_list_destroy(parameters);
        return NULL;
    }

    if (!lstf_parser_expect_token(parser, lstf_token_openbrace, error)) {
        free(function_name);
        ptr_list_destroy(parameters);
        lstf_codenode_unref(return_type);
        return NULL;
    }

    ptr_list *statements = lstf_parser_parse_statement_list(parser, false);

    if (!lstf_parser_expect_token(parser, lstf_token_closebrace, error)) {
        free(function_name);
        ptr_list_destroy(parameters);
        lstf_codenode_unref(return_type);
        ptr_list_destroy(statements);
        return NULL;
    }

    lstf_function *function = (lstf_function *)
        lstf_function_new(&(lstf_sourceref) {
                parser->file,
                begin,
                end,
            }, function_name, return_type, in_class_declaration, false, is_async);

    for (iterator params_it = ptr_list_iterator_create(parameters);
            params_it.has_next; params_it = iterator_next(params_it))
        lstf_function_add_parameter(function, (lstf_variable *)iterator_get_item(params_it));

    for (iterator stmts_it = ptr_list_iterator_create(statements);
            stmts_it.has_next; stmts_it = iterator_next(stmts_it))
        lstf_function_add_statement(function, (lstf_statement *)iterator_get_item(stmts_it));

    ptr_list_destroy(parameters);
    ptr_list_destroy(statements);
    free(function_name);

    return lstf_declaration_new_from_function(&lstf_codenode_cast(function)->source_reference, function);
}

static lstf_statement *
lstf_parser_parse_interface_declaration(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);
    
    if (!lstf_parser_expect_token(parser, lstf_token_keyword_interface, error))
        return NULL;

    char *interface_name = lstf_parser_parse_identifier(parser, error);
    if (!interface_name)
        return NULL;

    ptr_list *base_types = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);
    if (lstf_parser_accept_token(parser, lstf_token_keyword_extends)) {
        while (true) {
            lstf_sourceloc type_name_begin = lstf_scanner_get_location(parser->scanner);
            char *type_name = lstf_parser_parse_identifier(parser, error);
            if (!type_name)
                break;
            ptr_list_append(base_types, lstf_unresolvedtype_new(&(lstf_sourceref) {
                            parser->file,
                            type_name_begin,
                            lstf_scanner_get_prev_end_location(parser->scanner)
                        }, type_name));
            free(type_name);
            if (!lstf_parser_accept_token(parser, lstf_token_comma))
                break;
        }
    }

    if (*error || !lstf_parser_expect_token(parser, lstf_token_openbrace, error)) {
        free(interface_name);
        ptr_list_destroy(base_types);
        return NULL;
    }

    ptr_list *properties_list = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);
    while (lstf_scanner_current(parser->scanner) == lstf_token_identifier) {
        lstf_sourceloc property_begin = lstf_scanner_get_location(parser->scanner);
        char *property_name = lstf_parser_parse_identifier(parser, error);
        bool is_optional = lstf_parser_accept_token(parser, lstf_token_questionmark);

        if (!lstf_parser_expect_token(parser, lstf_token_colon, error)) {
            free(property_name);
            break;
        }

        lstf_datatype *property_type = lstf_parser_parse_data_type(parser, error);
        if (!property_type) {
            free(property_name);
            break;
        }

        ptr_list_append(properties_list, lstf_interfaceproperty_new(&(lstf_sourceref) {
                        parser->file,
                        property_begin,
                        lstf_scanner_get_prev_end_location(parser->scanner)
                    }, property_name, is_optional, property_type, false));
        free(property_name);
        if (!lstf_parser_accept_token(parser, lstf_token_semicolon))
            break;
    }

    if (*error || !lstf_parser_expect_token(parser, lstf_token_closebrace, error)) {
        free(interface_name);
        ptr_list_destroy(base_types);
        ptr_list_destroy(properties_list);
        return NULL;
    }

    lstf_interface *interface = lstf_interface_new(&(lstf_sourceref) {
                parser->file,
                begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            }, interface_name, false, false);

    for (iterator it = ptr_list_iterator_create(base_types); it.has_next; it = iterator_next(it))
        lstf_interface_add_base_type(interface, (lstf_datatype *)iterator_get_item(it));

    for (iterator it = ptr_list_iterator_create(properties_list); it.has_next; it = iterator_next(it))
        lstf_interface_add_member(interface, (lstf_interfaceproperty *)iterator_get_item(it));

    free(interface_name);
    ptr_list_destroy(base_types);
    ptr_list_destroy(properties_list);

    return lstf_declaration_new_from_interface(&(lstf_sourceref) {
                parser->file,
                begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            }, interface);
}

static lstf_statement *
lstf_parser_parse_type_alias_declaration(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);

    if (!lstf_parser_expect_token(parser, lstf_token_keyword_type, error))
        return NULL;

    char *type_name = lstf_parser_parse_identifier(parser, error);
    if (!type_name)
        return NULL;

    if (!lstf_parser_expect_token(parser, lstf_token_assignment, error)) {
        free(type_name);
        return NULL;
    }

    lstf_datatype *aliased_type = lstf_parser_parse_data_type(parser, error);
    if (!aliased_type || !lstf_parser_expect_token(parser, lstf_token_semicolon, error)) {
        free(type_name);
        lstf_codenode_unref(aliased_type);
        return NULL;
    }

    lstf_typealias *type_alias = (lstf_typealias *)
        lstf_typealias_new(&(lstf_sourceref) {
                parser->file,
                begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            }, type_name, aliased_type, false);
    free(type_name);

    return lstf_declaration_new_from_type_alias(&(lstf_sourceref) {
                parser->file,
                begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            }, type_alias);
}

static lstf_statement *
lstf_parser_parse_declaration_statement(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);

    switch (lstf_scanner_current(parser->scanner)) {
    case lstf_token_keyword_enum:
        return lstf_parser_parse_enum_declaration(parser, error);
    case lstf_token_keyword_async:
    case lstf_token_keyword_fun:
        return lstf_parser_parse_function_declaration(parser, false, error);
    case lstf_token_keyword_interface:
        return lstf_parser_parse_interface_declaration(parser, error);
    case lstf_token_keyword_type:
        return lstf_parser_parse_type_alias_declaration(parser, error);
    default:
        *error = lstf_parsererror_new(
                &lstf_sourceref_at_location(parser->file, begin),
                "expected declaration, got %s", lstf_token_to_string(lstf_scanner_current(parser->scanner)));
        break;
    }

    return NULL;
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
            next_expr = lstf_parser_parse_member_access_expression(parser, expression, error);
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
lstf_parser_parse_return_statement(lstf_parser *parser, lstf_parsererror **error)
{
    lstf_sourceloc begin = lstf_scanner_get_location(parser->scanner);

    if (!lstf_parser_expect_token(parser, lstf_token_keyword_return, error))
        return NULL;

    lstf_expression *expression = NULL;

    if (!lstf_parser_accept_token(parser, lstf_token_semicolon)) {
        if (!(expression = lstf_parser_parse_expression(parser, error)))
            return NULL;

        if (!lstf_parser_expect_token(parser, lstf_token_semicolon, error)) {
            lstf_codenode_unref(expression);
            return NULL;
        }
    }
    
    return lstf_returnstatement_new(&(lstf_sourceref) {
                parser->file,
                begin,
                lstf_scanner_get_prev_end_location(parser->scanner)
            }, expression);
}

static lstf_statement *
lstf_parser_parse_statement(lstf_parser *parser, lstf_parsererror **error)
{
    switch (lstf_scanner_current(parser->scanner)) {
    case lstf_token_keyword_let:
        return lstf_parser_parse_assignment_statement(parser, error);
    case lstf_token_identifier:
    {
        const unsigned saved_token_idx = parser->scanner->current_token_idx;
        lstf_token next_token = lstf_scanner_peek_next(parser->scanner);

        if (next_token == lstf_token_eof) {
            *error = lstf_parsererror_new(
                    &lstf_sourceref_at_location(parser->file, 
                        lstf_scanner_get_location(parser->scanner)), 
                    "expected statement, got %s", lstf_token_to_string(next_token)); 
            return NULL;
        }

        while (!(next_token == lstf_token_assignment || next_token == lstf_token_equivalent ||
                    next_token == lstf_token_semicolon || next_token == lstf_token_keyword_let ||
                    next_token == lstf_token_keyword_await || next_token == lstf_token_keyword_const ||
                    next_token == lstf_token_keyword_for || next_token == lstf_token_eof)) {
            next_token = lstf_scanner_next(parser->scanner);
        }

        lstf_scanner_rewind(parser->scanner, saved_token_idx);

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
    case lstf_token_keyword_enum:
    case lstf_token_keyword_async:
    case lstf_token_keyword_fun:
    case lstf_token_keyword_interface:
    case lstf_token_keyword_type:
        return lstf_parser_parse_declaration_statement(parser, error);
    case lstf_token_keyword_return:
        return lstf_parser_parse_return_statement(parser, error);
    default:
        *error = lstf_parsererror_new(
                &lstf_sourceref_at_location(parser->file,
                    lstf_scanner_get_location(parser->scanner)),
                "expected beginning of statement, got %s", 
                lstf_token_to_string(lstf_scanner_current(parser->scanner)));
        break;
    }

    return NULL;
}

/**
 * Returns a list of `(lstf_statement *)` objects
 */
static ptr_list *
lstf_parser_parse_statement_list(lstf_parser *parser, bool in_root_scope)
{
    ptr_list *statements = ptr_list_new((collection_item_ref_func) lstf_codenode_ref, 
            (collection_item_unref_func) lstf_codenode_unref);

    lstf_statement *stmt = NULL;
    lstf_parsererror *error = NULL;

    while (lstf_scanner_current(parser->scanner) != lstf_token_eof && 
            (in_root_scope || lstf_scanner_current(parser->scanner) != lstf_token_closebrace) &&
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
                case lstf_token_keyword_for:
                case lstf_token_keyword_const:
                case lstf_token_keyword_async:
                case lstf_token_keyword_fun:
                case lstf_token_keyword_class:
                case lstf_token_keyword_enum:
                case lstf_token_keyword_interface:
                case lstf_token_keyword_type:
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
    ptr_list *statements = lstf_parser_parse_statement_list(parser, true);

    for (iterator it = ptr_list_iterator_create(statements); it.has_next; it = iterator_next(it))
        lstf_block_add_statement(parser->file->main_block, iterator_get_item(it));

    ((lstf_codenode *)parser->file->main_block)->source_reference = (lstf_sourceref) {
        parser->file,
        begin,
        lstf_scanner_get_location(parser->scanner)
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
