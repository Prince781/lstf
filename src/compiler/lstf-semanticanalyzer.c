#include "lstf-semanticanalyzer.h"
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
#include <assert.h>
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

        if (!element_type)
            element_type = lstf_anytype_new(&((lstf_codenode *)array)->source_reference);

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
    if (assign->is_declaration) {
        lstf_variable *variable = lstf_variable_cast(assign->lhs->symbol_reference);

        if (!variable) {
            lstf_report_error(&lstf_codenode_cast(assign)->source_reference,
                    "left hand of assignment must be a variable");
            analyzer->num_errors++;
            return;
        }

        if (!variable->variable_type) {
            // the variable does not have an explicit type, so set
            // it to the type of the RHS

            // resolve RHS first
            bool old_ellipsis_allowed = analyzer->ellipsis_allowed;
            analyzer->ellipsis_allowed = false;
            lstf_codenode_accept(assign->rhs, visitor);
            analyzer->ellipsis_allowed = old_ellipsis_allowed;

            if (assign->rhs->value_type) {
                lstf_variable_set_variable_type(variable, assign->rhs->value_type);
                // resolve LHS
                lstf_expression_set_value_type(assign->lhs, assign->rhs->value_type);
            }
        } else {
            // the variable *does* have an explicit type
            if (!assign->lhs->value_type)
                lstf_expression_set_value_type(assign->lhs, variable->variable_type);
            // resolve RHS
            bool old_ellipsis_allowed = analyzer->ellipsis_allowed;
            analyzer->ellipsis_allowed = false;
            ptr_list_append(analyzer->expected_expression_types, variable->variable_type);
            lstf_codenode_accept(assign->rhs, visitor);
            ptr_list_remove_last_link(analyzer->expected_expression_types);
            analyzer->ellipsis_allowed = old_ellipsis_allowed;
        }
    } else {
        bool old_ellipsis_allowed = analyzer->ellipsis_allowed;
        analyzer->ellipsis_allowed = false;
        lstf_codenode_accept(assign->lhs, visitor);
        ptr_list_append(analyzer->expected_expression_types, assign->lhs->value_type);
        lstf_codenode_accept(assign->rhs, visitor);
        ptr_list_remove_last_link(analyzer->expected_expression_types);
        analyzer->ellipsis_allowed = old_ellipsis_allowed;
    }

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

    ptr_list_append(analyzer->scopes, block->scope);
    lstf_codenode_accept_children(block, visitor);
    ptr_list_remove_last_link(analyzer->scopes);
}

static void
lstf_semanticanalyzer_visit_declaration(lstf_codevisitor *visitor, lstf_declaration *decl)
{
    lstf_codenode_accept_children(decl, visitor);
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

        if (!access->inner->value_type) {
            lstf_report_error(&lstf_codenode_cast(access->inner)->source_reference,
                    "cannot access member `%s' of invalid expression",
                    access->member_name);
            analyzer->num_errors++;
            return;
        }

        // now determine how to interpret the member access
        switch (access->inner->value_type->datatype_type) {
        case lstf_datatype_type_anytype:
        case lstf_datatype_type_objecttype:
            lstf_report_warning(&lstf_codenode_cast(access)->source_reference, "unchecked member access"); 
            break;
        case lstf_datatype_type_interfacetype:
            break;
        default:
            lstf_report_error(&lstf_codenode_cast(access)->source_reference,
                    "request for member `%s' in something not an object", access->member_name);
            analyzer->num_errors++;
            break;
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
                                lstf_typesymbol_cast(expected_itype->interface), property_name))) {
                    for (iterator it2 = ptr_list_iterator_create(expected_itype->interface->extends_types);
                            it2.has_next; it2 = iterator_next(it2)) {
                        lstf_interfacetype *extends_type = iterator_get_item(it2);
                        if ((found_member = lstf_typesymbol_get_member(
                                        lstf_typesymbol_cast(extends_type->interface), property_name)))
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
lstf_semanticanalyzer_visit_return_statement(lstf_codevisitor *visitor, lstf_returnstatement *stmt)
{
    lstf_codenode_accept_children(stmt, visitor);
}

static const lstf_codevisitor_vtable semanticanalyzer_vtable = {
    lstf_semanticanalyzer_visit_array,
    lstf_semanticanalyzer_visit_assignment,
    lstf_semanticanalyzer_visit_block,
    NULL /* visit_constant */,
    NULL /* visit_data_type */,
    lstf_semanticanalyzer_visit_declaration,
    NULL /* visit_element_access */,
    lstf_semanticanalyzer_visit_ellipsis,
    NULL /* visit_enum */,
    lstf_semanticanalyzer_visit_expression,
    lstf_semanticanalyzer_visit_expression_statement,
    lstf_semanticanalyzer_visit_file,
    NULL /* visit_function */,
    NULL /* visit_interface */,
    NULL /* visit_interface_property */,
    lstf_semanticanalyzer_visit_literal,
    lstf_semanticanalyzer_visit_member_access,
    NULL /* visit_method_call */,
    lstf_semanticanalyzer_visit_object,
    lstf_semanticanalyzer_visit_object_property,
    NULL /* visit_pattern_test */,
    lstf_semanticanalyzer_visit_return_statement,
    NULL /* visit_type_alias */,
    NULL /* visit_variable */
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
    free(analyzer);
}
