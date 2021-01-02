#include "lstf-semanticanalyzer.h"
#include "lstf-patterntest.h"
#include "lstf-enumtype.h"
#include "lstf-patterntype.h"
#include "lstf-sourceref.h"
#include "lstf-returnstatement.h"
#include "lstf-declaration.h"
#include "lstf-ellipsis.h"
#include "lstf-enum.h"
#include "lstf-nulltype.h"
#include "lstf-functiontype.h"
#include "lstf-constant.h"
#include "lstf-function.h"
#include "lstf-interface.h"
#include "lstf-interfacetype.h"
#include "lstf-memberaccess.h"
#include "lstf-scope.h"
#include "lstf-symbol.h"
#include "lstf-typesymbol.h"
#include "data-structures/ptr-hashmap.h"
#include "data-structures/string-builder.h"
#include "lstf-object.h"
#include "lstf-booleantype.h"
#include "lstf-doubletype.h"
#include "lstf-expressionstatement.h"
#include "lstf-integertype.h"
#include "lstf-literal.h"
#include "lstf-stringtype.h"
#include "lstf-block.h"
#include "lstf-file.h"
#include "lstf-report.h"
#include "lstf-variable.h"
#include "lstf-assignment.h"
#include "lstf-uniontype.h"
#include "lstf-arraytype.h"
#include "lstf-expression.h"
#include "lstf-anytype.h"
#include "lstf-datatype.h"
#include "data-structures/iterator.h"
#include "lstf-array.h"
#include "lstf-codenode.h"
#include "lstf-codevisitor.h"
#include "data-structures/ptr-list.h"
#include "util.h"
#include "json/json.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

static lstf_datatype *
lstf_semanticanalyzer_get_current_expected_type(lstf_semanticanalyzer *analyzer)
{
    if (ptr_list_is_empty(analyzer->expected_expression_types))
        return NULL;
    return ptr_list_node_get_data(analyzer->expected_expression_types->tail, lstf_datatype *);
}

static void
lstf_semanticanalyzer_visit_array(lstf_codevisitor *visitor, lstf_array *array)
{
    lstf_semanticanalyzer *analyzer = (lstf_semanticanalyzer *)visitor;

    lstf_datatype *old_expected_dt = lstf_semanticanalyzer_get_current_expected_type(analyzer);
    if (old_expected_dt && old_expected_dt->datatype_type == lstf_datatype_type_arraytype)
        ptr_list_append(analyzer->expected_expression_types, 
                lstf_arraytype_cast(old_expected_dt)->element_type);
    else
        ptr_list_append(analyzer->expected_expression_types, NULL);

    // compute the [value_type]s of the element expressions first
    bool old_ellipsis_allowed = analyzer->ellipsis_allowed;
    analyzer->ellipsis_allowed = array->is_pattern;

    lstf_codenode_accept_children(array, visitor);
    ptr_list_remove_last_link(analyzer->expected_expression_types);

    if (!array->is_pattern) {
        lstf_datatype *element_type = NULL;
        bool created_new_union_type = false;

        for (iterator it = lstf_array_iterator_create(array); it.has_next; it = iterator_next(it)) {
            lstf_expression *element = iterator_get_item(it);

            if (!element->value_type) {
                // the array element is an invalid expression, 
                // so we bail out
                if (created_new_union_type)
                    lstf_codenode_unref(element_type);
                analyzer->ellipsis_allowed = old_ellipsis_allowed;
                return;
            }

            if (!element_type || lstf_datatype_equals(element_type, element->value_type)) {
                element_type = element->value_type;
            } else if (!created_new_union_type) {
                element_type = lstf_uniontype_new(&lstf_codenode_cast(element_type)->source_reference,
                        element_type, element->value_type, NULL);
                created_new_union_type = true;
            } else {
                lstf_uniontype_add_option(lstf_uniontype_cast(element_type), element->value_type);
            }
        }

        if (!element_type) {
            if (old_expected_dt && old_expected_dt->datatype_type == lstf_datatype_type_arraytype)
                element_type = lstf_arraytype_cast(old_expected_dt)->element_type;
            else
                element_type = lstf_anytype_new(&((lstf_codenode *)array)->source_reference);
        }

        lstf_expression_set_value_type(lstf_expression_cast(array), 
                lstf_arraytype_new(&lstf_codenode_cast(array)->source_reference, element_type));
    } else {
        lstf_expression_set_value_type(lstf_expression_cast(array),
                lstf_patterntype_new(&lstf_codenode_cast(array)->source_reference));
    }

    analyzer->ellipsis_allowed = old_ellipsis_allowed;
}

static void
lstf_semanticanalyzer_visit_assignment(lstf_codevisitor *visitor, lstf_assignment *assign)
{
    lstf_semanticanalyzer *analyzer = (lstf_semanticanalyzer *)visitor;
    lstf_scope *current_scope = ptr_list_node_get_data(analyzer->scopes->tail, lstf_scope *);

    // resolve LHS
    bool old_ellipsis_allowed = analyzer->ellipsis_allowed;
    analyzer->ellipsis_allowed = false;
    lstf_codenode_accept(assign->lhs, visitor);
    ptr_list_append(analyzer->expected_expression_types, assign->lhs->value_type);
    lstf_codenode_accept(assign->rhs, visitor);
    ptr_list_remove_last_link(analyzer->expected_expression_types);
    analyzer->ellipsis_allowed = old_ellipsis_allowed;

    if (assign->lhs->symbol_reference == lstf_scope_lookup(current_scope, "server_path"))
        analyzer->encountered_server_path_assignment = true;

    if (assign->lhs->symbol_reference == lstf_scope_lookup(current_scope, "project_files"))
        analyzer->encountered_project_files_assignment = true;

    if (!assign->lhs->value_type) {
        return;
    }

    if (!assign->rhs->value_type) {
        return;
    }
}

static void
lstf_semanticanalyzer_visit_block(lstf_codevisitor *visitor, lstf_block *block)
{
    lstf_semanticanalyzer *analyzer = (lstf_semanticanalyzer *)visitor;
    lstf_function *parent_function = lstf_function_cast(lstf_codenode_cast(block)->parent_node);
    
    if (parent_function)
        ptr_list_append(analyzer->expected_return_types, parent_function->return_type);
    ptr_list_append(analyzer->scopes, block->scope);
    lstf_codenode_accept_children(block, visitor);
    ptr_list_remove_last_link(analyzer->scopes);
    if (parent_function)
        ptr_list_remove_last_link(analyzer->expected_return_types);
}

static void
lstf_semanticanalyzer_visit_constant(lstf_codevisitor *visitor, lstf_constant *constant)
{
    lstf_codenode_accept_children(constant, visitor);
}

static void
lstf_semanticanalyzer_visit_declaration(lstf_codevisitor *visitor, lstf_declaration *decl)
{
    lstf_codenode_accept_children(decl, visitor);
}

static void
lstf_semanticanalyzer_visit_element_access(lstf_codevisitor *visitor, lstf_elementaccess *access)
{
    lstf_semanticanalyzer *analyzer = (lstf_semanticanalyzer *)visitor;

    lstf_codenode_accept(access->container, visitor);

    if (!access->container->value_type)
        return;

    // check expression is array, object, or string
    switch (access->container->value_type->datatype_type) {
    case lstf_datatype_type_arraytype:
    case lstf_datatype_type_objecttype:
    case lstf_datatype_type_interfacetype:
    case lstf_datatype_type_stringtype:
        break;
    default:
    {
        char *container_dt = lstf_datatype_to_string(access->container->value_type);
        lstf_report_error(&lstf_codenode_cast(access->container)->source_reference,
                "container expression of type `%s' is not an array, object, or string",
                container_dt);
        analyzer->num_errors++;
        free(container_dt);
        return;
    } break;
    }

    // check that the access indices are appropriate for the container
    // expression
    if (access->container->value_type->datatype_type == lstf_datatype_type_arraytype) {
        // array indices must be integers
        ptr_list_append(analyzer->expected_expression_types, lstf_integertype_new(NULL));
        for (iterator it = ptr_list_iterator_create(access->arguments);
                it.has_next; it = iterator_next(it))
            lstf_codenode_accept(iterator_get_item(it), visitor);
        ptr_list_remove_last_link(analyzer->expected_expression_types);

        if (access->arguments->length == 2) {
            // this is an array slice
            lstf_expression_set_value_type(lstf_expression_cast(access), access->container->value_type);
        } else if (access->arguments->length == 1) {
            lstf_expression_set_value_type(lstf_expression_cast(access),
                    lstf_arraytype_cast(access->container->value_type)->element_type);
        } else {
            lstf_report_error(&lstf_codenode_cast(access)->source_reference,
                    "too many indices for array access");
            analyzer->num_errors++;
            return;
        }
    } else if (access->container->value_type->datatype_type == lstf_datatype_type_objecttype) {
        // object indices must be strings
        ptr_list_append(analyzer->expected_expression_types, lstf_stringtype_new(NULL));
        for (iterator it = ptr_list_iterator_create(access->arguments);
                it.has_next; it = iterator_next(it))
            lstf_codenode_accept(iterator_get_item(it), visitor);
        ptr_list_remove_last_link(analyzer->expected_expression_types);

        // unchecked access to plain 'object'
        if (access->arguments->length == 1) {
            lstf_expression_set_value_type(lstf_expression_cast(access),
                    lstf_anytype_new(&lstf_codenode_cast(access)->source_reference));
        } else {
            lstf_report_error(&lstf_codenode_cast(access)->source_reference,
                    "can only access one property at a time");
            analyzer->num_errors++;
            return;
        }
    } else if (access->container->value_type->datatype_type == lstf_datatype_type_interfacetype) {
        // interface (object) indices must be strings
        ptr_list_append(analyzer->expected_expression_types, lstf_stringtype_new(NULL));
        for (iterator it = ptr_list_iterator_create(access->arguments);
                it.has_next; it = iterator_next(it))
            lstf_codenode_accept(iterator_get_item(it), visitor);
        ptr_list_remove_last_link(analyzer->expected_expression_types);

        // checked access to interface
        if (access->arguments->length == 1) {
            lstf_expression *property_expr = iterator_get_item(ptr_list_iterator_create(access->arguments));
            lstf_literal *property_lit = lstf_literal_cast(property_expr);
            lstf_interface *first_interface = lstf_interface_cast(access->container->value_type->symbol);

            if (property_lit && property_lit->literal_type == lstf_literal_type_string) {
                const char *property_name = lstf_literal_get_string(property_lit);
                char *canon_property_name = json_member_name_canonicalize(property_name);
                lstf_symbol *found_member = NULL;

                ptr_list *interfaces_to_check = ptr_list_new(NULL, NULL);
                ptr_list_append(interfaces_to_check, first_interface);

                // compare this canonicalized property name to the
                // canonicalized property names of the interface(s) we are
                // checking
                while (!found_member && !ptr_list_is_empty(interfaces_to_check)) {
                    lstf_typesymbol *interface = ptr_list_remove_first_link(interfaces_to_check);

                    for (iterator it = ptr_hashmap_iterator_create(interface->members);
                            !found_member && it.has_next; it = iterator_next(it)) {
                        const ptr_hashmap_entry *entry = iterator_get_item(it);
                        const char *other_property_name = entry->key;
                        char *other_canon_property_name = json_member_name_canonicalize(other_property_name);

                        if (strcmp(canon_property_name, other_canon_property_name) == 0)
                            found_member = entry->value;

                        free(other_canon_property_name);
                    }

                    for (iterator it = ptr_list_iterator_create(lstf_interface_cast(interface)->extends_types);
                            it.has_next; it = iterator_next(it))
                        ptr_list_append(interfaces_to_check, lstf_datatype_cast(iterator_get_item(it))->symbol);
                }

                if (!found_member) {
                    char *dt_string = lstf_datatype_to_string(access->container->value_type);

                    if (strcmp(property_name, canon_property_name) == 0) {
                        lstf_report_error(&lstf_codenode_cast(property_lit)->source_reference,
                                "`%s' is not a member of type `%s'",
                                property_name,
                                dt_string);
                    } else {
                        lstf_report_error(&lstf_codenode_cast(property_lit)->source_reference,
                                "`%s'/`%s' is not a member of type `%s'",
                                property_name, canon_property_name,
                                dt_string);
                    }
                    analyzer->num_errors++;

                    free(dt_string);
                    ptr_list_destroy(interfaces_to_check);
                    free(canon_property_name);
                    return;
                }

                ptr_list_destroy(interfaces_to_check);
                free(canon_property_name);
            }
        } else {
            lstf_report_error(&lstf_codenode_cast(access)->source_reference,
                    "can only access one property at a time");
            analyzer->num_errors++;
            return;
        }
    } else if (access->container->value_type->datatype_type == lstf_datatype_type_stringtype) {
        // string (char array) indices must be integers
        ptr_list_append(analyzer->expected_expression_types, lstf_integertype_new(NULL));
        for (iterator it = ptr_list_iterator_create(access->arguments);
                it.has_next; it = iterator_next(it))
            lstf_codenode_accept(iterator_get_item(it), visitor);
        ptr_list_remove_last_link(analyzer->expected_expression_types);

        if (access->arguments->length >= 1 && access->arguments->length <= 2) {
            // this is a string slice
            lstf_expression_set_value_type(lstf_expression_cast(access), access->container->value_type);
        } else {
            lstf_report_error(&lstf_codenode_cast(access)->source_reference,
                    "too many indices for substring access (expected 1 or 2)");
            analyzer->num_errors++;
            return;
        }
    }
}

static void
lstf_semanticanalyzer_visit_ellipsis(lstf_codevisitor *visitor, lstf_ellipsis *ellipsis)
{
    lstf_semanticanalyzer *analyzer = (lstf_semanticanalyzer *)visitor;

    if (!analyzer->ellipsis_allowed) {
        lstf_report_error(&lstf_codenode_cast(ellipsis)->source_reference,
                "ellipsis not allowed in this context");
        analyzer->num_errors++;
    }
}

static void
lstf_semanticanalyzer_visit_enum(lstf_codevisitor *visitor, lstf_enum *enum_symbol)
{
    lstf_codenode_accept_children(enum_symbol, visitor);

    lstf_datatype *members_type = NULL;
    bool created_new_union_type = false;

    for (iterator it = ptr_hashmap_iterator_create(lstf_typesymbol_cast(enum_symbol)->members);
            it.has_next; it = iterator_next(it)) {
        const ptr_hashmap_entry *entry = iterator_get_item(it);
        lstf_constant *constant = entry->value;

        if (!members_type || lstf_datatype_equals(members_type, constant->expression->value_type)) {
            members_type = constant->expression->value_type;
        } else if (!created_new_union_type) {
            members_type = lstf_uniontype_new(&lstf_codenode_cast(enum_symbol)->source_reference, 
                    members_type, constant->expression->value_type, NULL);
            created_new_union_type = true;
        } else {
            lstf_uniontype_add_option(lstf_uniontype_cast(members_type), constant->expression->value_type);
        }
    }

    lstf_enum_set_members_type(enum_symbol, members_type);
}

static void
lstf_semanticanalyzer_visit_expression(lstf_codevisitor *visitor, lstf_expression *expr)
{
    lstf_semanticanalyzer *analyzer = (lstf_semanticanalyzer *)visitor;
    lstf_datatype *expected_type = lstf_semanticanalyzer_get_current_expected_type(analyzer);

    if (expected_type && expr->value_type) {
        // attempt to cast the expression type
        if (expr->value_type &&
                lstf_datatype_is_supertype_of(expected_type, expr->value_type)) {
            lstf_expression_set_value_type(expr, expected_type);
        } else {
            char *expr_type_to_string = lstf_datatype_to_string(expr->value_type);
            char *expected_et_to_string = lstf_datatype_to_string(expected_type);

            lstf_report_error(&lstf_codenode_cast(expr)->source_reference,
                    "cannot convert expression of type `%s' to `%s'",
                    expr_type_to_string, expected_et_to_string);
            analyzer->num_errors++;
            free(expr_type_to_string);
            free(expected_et_to_string);
        }
    }
}

static void
lstf_semanticanalyzer_visit_expression_statement(lstf_codevisitor *visitor, lstf_expressionstatement *stmt)
{
    lstf_codenode_accept_children(stmt, visitor);
}

static void
lstf_semanticanalyzer_visit_file(lstf_codevisitor *visitor, lstf_file *file)
{
    lstf_semanticanalyzer *analyzer = (lstf_semanticanalyzer *)visitor;

    lstf_codenode_accept(file->main_block, visitor);

    if (!analyzer->encountered_server_path_assignment) {
        lstf_report_error(&lstf_sourceref_default_from_file(file),
                "assignment to `server_path' required");
        analyzer->num_errors++;
    }

    if (!analyzer->encountered_project_files_assignment) {
        lstf_report_error(&lstf_sourceref_default_from_file(file),
                "assignment to `project_files' required");
        analyzer->num_errors++;
    }
}

static void
lstf_semanticanalyzer_visit_function(lstf_codevisitor *visitor, lstf_function *function)
{
    lstf_codenode_accept_children(function, visitor);
}

static void
lstf_semanticanalyzer_visit_literal(lstf_codevisitor *visitor, lstf_literal *lit)
{
    (void) visitor;
    lstf_datatype *expr_type = NULL;

    switch (lit->literal_type) {
        case lstf_literal_type_boolean:
            expr_type = lstf_booleantype_new(&lstf_codenode_cast(lit)->source_reference);
            break;
        case lstf_literal_type_double:
            expr_type = lstf_doubletype_new(&lstf_codenode_cast(lit)->source_reference);
            break;
        case lstf_literal_type_integer:
            expr_type = lstf_integertype_new(&lstf_codenode_cast(lit)->source_reference);
            break;
        case lstf_literal_type_null:
            expr_type = lstf_nulltype_new(&lstf_codenode_cast(lit)->source_reference);
            break;
        case lstf_literal_type_string:
            expr_type = lstf_stringtype_new(&lstf_codenode_cast(lit)->source_reference);
            break;
    }

    lstf_expression_set_value_type(lstf_expression_cast(lit), expr_type);
}

static void
lstf_semanticanalyzer_visit_member_access(lstf_codevisitor *visitor, lstf_memberaccess *access)
{
    lstf_semanticanalyzer *analyzer = (lstf_semanticanalyzer *)visitor;

    lstf_codenode_accept_children(access, visitor);
    lstf_expression *expr = lstf_expression_cast(access);

    // first, attempt to resolve member access, based on the type
    // of the inner expression
    if (!expr->symbol_reference) {
        if (!access->inner) {
            // nothing to do: simple member accesses should've been resolved by
            // the symbol resolver
            return;
        }

        if (!access->inner->value_type && !access->inner->symbol_reference) {
            lstf_report_error(&lstf_codenode_cast(access->inner)->source_reference,
                    "cannot access member `%s' of invalid expression",
                    access->member_name);
            analyzer->num_errors++;
            return;
        }

        // now determine how to interpret the member access

        if (!access->inner->value_type) {
            // access->inner has a symbol_reference, but no type
            // This should only be possible if the symbol is a type symbol.
            assert(access->inner->symbol_reference->symbol_type == lstf_symbol_type_typesymbol &&
                    "untyped MA inner expression must reference a type symbol");
            expr->symbol_reference =
                lstf_typesymbol_get_member(
                        lstf_typesymbol_cast(access->inner->symbol_reference),
                        access->member_name);
            if (!expr->symbol_reference) {
                lstf_report_error(&lstf_codenode_cast(access)->source_reference,
                        "`%s' is not a member of `%s'",
                        access->member_name,
                        access->inner->symbol_reference->name);
                analyzer->num_errors++;
                return;
            }
        } else {
            switch (access->inner->value_type->datatype_type) {
            case lstf_datatype_type_interfacetype:
            {
                expr->symbol_reference = 
                    lstf_typesymbol_get_member(
                        lstf_typesymbol_cast(access->inner->value_type->symbol), 
                        access->member_name);
                
                if (!expr->symbol_reference) {
                    char *dt_string = lstf_datatype_to_string(access->inner->value_type);
                    lstf_report_error(&lstf_codenode_cast(access)->source_reference,
                            "property `%s' does not exist on type `%s'",
                            access->member_name,
                            dt_string);
                    analyzer->num_errors++;
                    free(dt_string);
                    return;
                }
            } break;
            case lstf_datatype_type_uniontype:
                lstf_report_note(&lstf_codenode_cast(access)->source_reference,
                        "this will be checked in the next stage");
                break;
            case lstf_datatype_type_objecttype:
                /* XXX: create dynamic type check here? */
            case lstf_datatype_type_anytype:        // XXX: here too?
            case lstf_datatype_type_booleantype:
            case lstf_datatype_type_doubletype:
            case lstf_datatype_type_enumtype:
            case lstf_datatype_type_functiontype:
            case lstf_datatype_type_integertype:
            case lstf_datatype_type_nulltype:
            case lstf_datatype_type_voidtype:
            case lstf_datatype_type_stringtype:
            case lstf_datatype_type_patterntype:
            case lstf_datatype_type_numbertype:
            case lstf_datatype_type_arraytype:
                lstf_report_error(&lstf_codenode_cast(access)->source_reference,
                        "request for member `%s' in something not an object", access->member_name);
                analyzer->num_errors++;
                break;
            case lstf_datatype_type_unresolvedtype:
                fprintf(stderr, "%s: unresolved type found in semantic analysis!", __func__);
                abort();
            }
        }
    }

    // then, attempt to discover the expression's value type
    if (expr->symbol_reference && !expr->value_type) {
        switch (expr->symbol_reference->symbol_type) {
            case lstf_symbol_type_constant:
                lstf_expression_set_value_type(expr, 
                        lstf_expression_cast(lstf_constant_cast(expr->symbol_reference)->expression)->value_type);
                break;
            case lstf_symbol_type_function:
                lstf_expression_set_value_type(expr, 
                        lstf_functiontype_new_from_function(&lstf_codenode_cast(expr)->source_reference,
                            lstf_function_cast(expr->symbol_reference)));
                break;
            case lstf_symbol_type_interfaceproperty:
                lstf_expression_set_value_type(expr,
                        lstf_interfaceproperty_cast(expr->symbol_reference)->property_type);
                break;
            case lstf_symbol_type_objectproperty:
                lstf_expression_set_value_type(expr,
                        lstf_objectproperty_cast(expr->symbol_reference)->value->value_type);
                break;
            case lstf_symbol_type_typesymbol:
                // the member access is an explicit reference to the type
                // symbol, and so it should not get a value type
                break;
            case lstf_symbol_type_variable:
                lstf_expression_set_value_type(expr,
                        lstf_variable_cast(expr->symbol_reference)->variable_type);
                break;
        }
    }
}

static void
lstf_semanticanalyzer_visit_method_call(lstf_codevisitor* visitor, lstf_methodcall* mcall)
{
    lstf_semanticanalyzer *analyzer = (lstf_semanticanalyzer*)visitor;

    lstf_codenode_accept(mcall->call, visitor);

    if (!mcall->call->value_type)
        return;

    // check expression is callable
    if (mcall->call->value_type->datatype_type != lstf_datatype_type_functiontype) {
        char *call_dt = lstf_datatype_to_string(mcall->call->value_type);
        lstf_report_error(&lstf_codenode_cast(mcall->call)->source_reference,
            "expression of type `%s' is not callable", call_dt);
        analyzer->num_errors++;
        free(call_dt);
        return;
    }

    lstf_functiontype *call_ft = lstf_functiontype_cast(mcall->call->value_type);

    // check arguments are correct
    iterator ft_paramname_it = ptr_list_iterator_create(call_ft->parameter_names);
    iterator ft_paramtype_it = ptr_list_iterator_create(call_ft->parameter_types);
    iterator call_argexpr_it = ptr_list_iterator_create(mcall->arguments);

    while (ft_paramname_it.has_next && ft_paramtype_it.has_next && call_argexpr_it.has_next) {
        lstf_datatype *param_type = iterator_get_item(ft_paramtype_it);
        lstf_expression *argument = iterator_get_item(call_argexpr_it);

        ptr_list_append(analyzer->expected_expression_types, param_type);
        lstf_codenode_accept(argument, visitor);
        ptr_list_remove_last_link(analyzer->expected_expression_types);

        ft_paramname_it = iterator_next(ft_paramname_it);
        ft_paramtype_it = iterator_next(ft_paramtype_it);
        call_argexpr_it = iterator_next(call_argexpr_it);
    }

    if (ft_paramtype_it.has_next && !call_argexpr_it.has_next) {
        lstf_report_error(&lstf_codenode_cast(mcall)->source_reference,
            "expected %u arguments to function instead of only %u",
            call_ft->parameter_types->length,
            mcall->arguments->length);
        analyzer->num_errors++;
        lstf_function *function = lstf_function_cast(mcall->call->value_type->symbol);
        if (function) {
            while (ft_paramname_it.has_next && ft_paramtype_it.has_next) {
                const char *param_name = iterator_get_item(ft_paramname_it);
                lstf_variable *parameter = lstf_function_get_parameter(function, param_name);

                lstf_report_note(&lstf_codenode_cast(parameter)->source_reference,
                        "parameter `%s' required",
                        param_name);

                ft_paramname_it = iterator_next(ft_paramname_it);
                ft_paramtype_it = iterator_next(ft_paramtype_it);
            }
        }
    } else if (!ft_paramtype_it.has_next && call_argexpr_it.has_next) {
        lstf_report_error(&lstf_codenode_cast(mcall)->source_reference,
            "expected %u arguments to function instead of %u",
            call_ft->parameter_types->length,
            mcall->arguments->length);
        analyzer->num_errors++;
    }

    lstf_expression_set_value_type(lstf_expression_cast(mcall), call_ft->return_type);
}

static void
lstf_semanticanalyzer_visit_object(lstf_codevisitor *visitor, lstf_object *object)
{
    lstf_semanticanalyzer *analyzer = (lstf_semanticanalyzer *)visitor;

    // check for duplicate members first
    ptr_hashmap *members_map = ptr_hashmap_new((collection_item_hash_func) strhash, 
            NULL, 
            NULL,
            (collection_item_equality_func) strequal,
            (collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);

    for (iterator it = lstf_object_iterator_create(object); it.has_next; it = iterator_next(it)) {
        lstf_objectproperty *property = iterator_get_item(it);
        char *property_name = lstf_symbol_cast(property)->name;
        const ptr_hashmap_entry *entry = NULL;
        if ((entry = ptr_hashmap_get(members_map, property_name))) {
            lstf_report_error(&lstf_codenode_cast(property)->source_reference,
                    "initializer conflicts with previous initializer of this property");
            lstf_report_note(&lstf_codenode_cast(entry->value)->source_reference,
                    "previous initialization is here");
            analyzer->num_errors++;
            ptr_hashmap_destroy(members_map);
            return;
        } else {
            ptr_hashmap_insert(members_map, property_name, property);
        }
    }

    ptr_hashmap_destroy(members_map);
    members_map = NULL;

    lstf_datatype *expected_et = lstf_semanticanalyzer_get_current_expected_type(analyzer);

    if (object->is_pattern && (!expected_et || expected_et->datatype_type == lstf_datatype_type_patterntype)) {
        // if we're expecting a pattern, then cast the properties to a pattern
        // type as well
        bool old_ellipsis_allowed = analyzer->ellipsis_allowed;
        analyzer->ellipsis_allowed = object->is_pattern;
        lstf_codenode_accept_children(object, visitor);
        analyzer->ellipsis_allowed = old_ellipsis_allowed;

        lstf_expression_set_value_type(lstf_expression_cast(object),
                lstf_patterntype_new(&lstf_codenode_cast(object)->source_reference));
    } else {
        // create a new anonymous interface type composed from the types
        // of the members of this object expression
        lstf_interface *anonymous_interface = 
            lstf_interface_new(&lstf_codenode_cast(object)->source_reference, NULL, true, false);

        for (iterator it = lstf_object_iterator_create(object); it.has_next; it = iterator_next(it)) {
            lstf_objectproperty *property = iterator_get_item(it);

            if (!expected_et || expected_et->datatype_type != lstf_datatype_type_interfacetype) {
                ptr_list_append(analyzer->expected_expression_types, NULL);
            } else {
                lstf_interfacetype *expected_itype = lstf_interfacetype_cast(expected_et);
                const char *property_name = lstf_symbol_cast(property)->name;
                lstf_symbol *found_member = NULL;

                // attempt to find the corresponding interface member,
                // whose type we want to cast this object property to
                if (!(found_member = lstf_typesymbol_get_member(
                                lstf_typesymbol_cast(lstf_datatype_cast(expected_itype)->symbol), property_name))) {
                    for (iterator it2 = ptr_list_iterator_create(
                                lstf_interface_cast(lstf_datatype_cast(expected_itype)->symbol)->extends_types);
                            it2.has_next; it2 = iterator_next(it2)) {
                        lstf_interfacetype *extends_type = iterator_get_item(it2);
                        if ((found_member = lstf_typesymbol_get_member(
                                        lstf_typesymbol_cast(lstf_datatype_cast(extends_type)->symbol), property_name)))
                            break;
                    }
                }

                lstf_interfaceproperty *interface_property = lstf_interfaceproperty_cast(found_member);

                if (interface_property)
                    ptr_list_append(analyzer->expected_expression_types, interface_property->property_type);
                else
                    ptr_list_append(analyzer->expected_expression_types, NULL);
            }
            lstf_codenode_accept(property, visitor);
            ptr_list_remove_last_link(analyzer->expected_expression_types);

            if (!property->value->value_type) {
                lstf_codenode_unref(anonymous_interface);
                return;
            }

            lstf_interface_add_member(anonymous_interface,
                    lstf_interfaceproperty_new(&lstf_codenode_cast(property)->source_reference,
                        lstf_symbol_cast(property)->name,
                        property->is_nullable,
                        property->value->value_type,
                        lstf_symbol_cast(anonymous_interface)->is_builtin));
        }

        lstf_scope *current_scope = ptr_list_node_get_data(analyzer->scopes->tail, lstf_scope *);
        lstf_scope_add_symbol(current_scope, lstf_symbol_cast(anonymous_interface));

        lstf_expression_set_value_type(lstf_expression_cast(object),
                lstf_interfacetype_new(&lstf_codenode_cast(object)->source_reference,
                    anonymous_interface));
    }
}

static void
lstf_semanticanalyzer_visit_object_property(lstf_codevisitor *visitor, lstf_objectproperty *property)
{
    lstf_codenode_accept_children(property, visitor);
}

static void
lstf_semanticanalyzer_visit_pattern_test(lstf_codevisitor *visitor, lstf_patterntest *stmt)
{
    lstf_codenode_accept_children(stmt, visitor);
}

static void
lstf_semanticanalyzer_visit_return_statement(lstf_codevisitor *visitor, lstf_returnstatement *stmt)
{
    lstf_semanticanalyzer *analyzer = (lstf_semanticanalyzer *)visitor;

    lstf_datatype *expression_rt = NULL;
    if (!ptr_list_is_empty(analyzer->expected_return_types))
        expression_rt = ptr_list_node_get_data(analyzer->expected_return_types->tail, lstf_datatype *);

    if (stmt->expression) {
        if (!expression_rt || expression_rt->datatype_type == lstf_datatype_type_voidtype) {
            lstf_report_error(&lstf_codenode_cast(stmt)->source_reference,
                    "return statement should not return a value here");
            analyzer->num_errors++;
        } else {
            ptr_list_append(analyzer->expected_expression_types, expression_rt);
            lstf_codenode_accept_children(stmt, visitor);
            ptr_list_remove_last_link(analyzer->expected_expression_types);
        }
    } else if (expression_rt && expression_rt->datatype_type != lstf_datatype_type_voidtype) {
        char *expression_rt_to_string = lstf_datatype_to_string(expression_rt);
        lstf_report_error(&lstf_codenode_cast(stmt)->source_reference,
                "cannot return void here (expected `%s')",
                expression_rt_to_string);
        analyzer->num_errors++;
        free(expression_rt_to_string);
    }
}

static void
lstf_semanticanalyzer_visit_variable(lstf_codevisitor *visitor, lstf_variable *variable)
{
    lstf_semanticanalyzer *analyzer = (lstf_semanticanalyzer *)visitor;

    if (!variable->variable_type) {
        // the variable does not have an explicit type, so set
        // it to the type of the RHS
        assert(variable->initializer && "untyped variable must have an initializer!");

        // resolve RHS first
        bool old_ellipsis_allowed = analyzer->ellipsis_allowed;
        analyzer->ellipsis_allowed = false;
        lstf_codenode_accept(variable->initializer, visitor);
        analyzer->ellipsis_allowed = old_ellipsis_allowed;

        if (variable->initializer->value_type)
            lstf_variable_set_variable_type(variable, variable->initializer->value_type);
    } else if (variable->initializer) {
        // the variable *does* have an explicit type
        // resolve RHS
        bool old_ellipsis_allowed = analyzer->ellipsis_allowed;
        analyzer->ellipsis_allowed = false;
        ptr_list_append(analyzer->expected_expression_types, variable->variable_type);
        lstf_codenode_accept(variable->initializer, visitor);
        ptr_list_remove_last_link(analyzer->expected_expression_types);
        analyzer->ellipsis_allowed = old_ellipsis_allowed;
    }
    
    if (variable->variable_type && variable->variable_type->datatype_type == lstf_datatype_type_voidtype) {
        char *dt_string = lstf_datatype_to_string(variable->variable_type);
        lstf_report_error(&lstf_codenode_cast(variable)->source_reference,
                "%s cannot have type `%s'",
                lstf_function_cast(lstf_codenode_cast(variable)->parent_node) ? "parameter" : "variable",
                dt_string);
        analyzer->num_errors++;
        free(dt_string);
        return;
    }
}

static const lstf_codevisitor_vtable semanticanalyzer_vtable = {
    lstf_semanticanalyzer_visit_array,
    lstf_semanticanalyzer_visit_assignment,
    lstf_semanticanalyzer_visit_block,
    lstf_semanticanalyzer_visit_constant,
    NULL /* visit_data_type */,
    lstf_semanticanalyzer_visit_declaration,
    lstf_semanticanalyzer_visit_element_access,
    lstf_semanticanalyzer_visit_ellipsis,
    lstf_semanticanalyzer_visit_enum,
    lstf_semanticanalyzer_visit_expression,
    lstf_semanticanalyzer_visit_expression_statement,
    lstf_semanticanalyzer_visit_file,
    lstf_semanticanalyzer_visit_function,
    NULL /* visit_interface */,
    NULL /* visit_interface_property */,
    lstf_semanticanalyzer_visit_literal,
    lstf_semanticanalyzer_visit_member_access,
    lstf_semanticanalyzer_visit_method_call,
    lstf_semanticanalyzer_visit_object,
    lstf_semanticanalyzer_visit_object_property,
    lstf_semanticanalyzer_visit_pattern_test,
    lstf_semanticanalyzer_visit_return_statement,
    NULL /* visit_type_alias */,
    lstf_semanticanalyzer_visit_variable
};

lstf_semanticanalyzer *lstf_semanticanalyzer_new(lstf_file *file)
{
    lstf_semanticanalyzer *analyzer = calloc(1, sizeof *analyzer);

    lstf_codevisitor_construct((lstf_codevisitor *)analyzer, &semanticanalyzer_vtable);
    analyzer->file = file;
    analyzer->scopes = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);
    analyzer->expected_expression_types = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);
    analyzer->expected_return_types = ptr_list_new((collection_item_ref_func) lstf_codenode_ref,
            (collection_item_unref_func) lstf_codenode_unref);

    return analyzer;
}

void lstf_semanticanalyzer_analyze(lstf_semanticanalyzer *analyzer)
{
    lstf_codevisitor_visit_file((lstf_codevisitor *)analyzer, analyzer->file);
}

void lstf_semanticanalyzer_destroy(lstf_semanticanalyzer *analyzer)
{
    analyzer->file = NULL;
    ptr_list_destroy(analyzer->scopes);
    analyzer->scopes = NULL;
    ptr_list_destroy(analyzer->expected_expression_types);
    analyzer->expected_expression_types = NULL;
    ptr_list_destroy(analyzer->expected_return_types);
    analyzer->expected_return_types = NULL;
    free(analyzer);
}
