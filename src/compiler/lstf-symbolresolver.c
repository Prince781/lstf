#include "lstf-symbolresolver.h"
#include "compiler/lstf-enum.h"
#include "compiler/lstf-functiontype.h"
#include "compiler/lstf-patterntype.h"
#include "compiler/lstf-voidtype.h"
#include "lstf-returnstatement.h"
#include "lstf-uniontype.h"
#include "lstf-declaration.h"
#include "lstf-interface.h"
#include "lstf-typealias.h"
#include "lstf-enumtype.h"
#include "lstf-interfacetype.h"
#include "data-structures/iterator.h"
#include "data-structures/ptr-hashmap.h"
#include "lstf-arraytype.h"
#include "lstf-objecttype.h"
#include "lstf-stringtype.h"
#include "lstf-booleantype.h"
#include "lstf-numbertype.h"
#include "lstf-doubletype.h"
#include "lstf-integertype.h"
#include "lstf-anytype.h"
#include "lstf-unresolvedtype.h"
#include "lstf-codevisitor.h"
#include "lstf-datatype.h"
#include "lstf-expression.h"
#include "lstf-patterntest.h"
#include "lstf-sourceref.h"
#include "lstf-statement.h"
#include "lstf-variable.h"
#include "lstf-methodcall.h"
#include "lstf-object.h"
#include "lstf-expressionstatement.h"
#include "lstf-function.h"
#include "data-structures/ptr-list.h"
#include "lstf-memberaccess.h"
#include "lstf-symbol.h"
#include "lstf-typesymbol.h"
#include "lstf-block.h"
#include "lstf-elementaccess.h"
#include "lstf-report.h"
#include "lstf-assignment.h"
#include "lstf-array.h"
#include "lstf-codenode.h"
#include "lstf-scope.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
lstf_symbolresolver_visit_array(lstf_codevisitor *visitor, lstf_array *array)
{
    lstf_codenode_accept_children(array, visitor);
}

static void
lstf_symbolresolver_visit_assignment(lstf_codevisitor *visitor, lstf_assignment *assign)
{
    lstf_symbolresolver *resolver = (lstf_symbolresolver *)visitor;
    lstf_scope *current_scope = ptr_list_node_get_data(resolver->scopes->tail, lstf_scope *);

    // resolve rhs before resolving lhs in case scope would be modified by declaration
    lstf_codenode_accept(assign->rhs, visitor);
    lstf_variable *lhs_declared = NULL;

    if (assign->is_declaration) {
        assert(assign->lhs->expr_type == lstf_expression_type_memberaccess &&
                ((lstf_memberaccess *)assign->lhs)->inner == NULL &&
                "`let' assignment must have identifier after keyword");

        lstf_symbol *lhs_variable = lstf_variable_new(&((lstf_codenode *)assign->lhs)->source_reference, 
                ((lstf_memberaccess *)assign->lhs)->member_name,
                false);

        lstf_symbol *clashing_var = NULL;
        if ((clashing_var = lstf_scope_lookup(current_scope, lhs_variable->name))) {
            if (clashing_var->is_builtin) {
                const char *symbol_type = "symbol";
                switch (clashing_var->symbol_type) {
                    case lstf_symbol_type_function:
                        symbol_type = "function";
                        break;
                    case lstf_symbol_type_typesymbol:
                        symbol_type = "type";
                        break;
                    case lstf_symbol_type_variable:
                        symbol_type = "variable";
                        break;
                    case lstf_symbol_type_objectproperty:
                    case lstf_symbol_type_interfaceproperty:
                        symbol_type = "property";
                        break;
                    case lstf_symbol_type_constant:
                        symbol_type = "constant";
                        break;
                }
                lstf_report_error(&((lstf_codenode *)lhs_variable)->source_reference, 
                        "redefinition of reserved %s `%s'", symbol_type, lhs_variable->name);
            } else {
                lstf_report_error(&((lstf_codenode *)lhs_variable)->source_reference, 
                        "redefinition of `%s'", lhs_variable->name);
                lstf_report_note(&((lstf_codenode *)clashing_var)->source_reference,
                        "previous definition of `%s' was here", lhs_variable->name);
            }
            resolver->num_errors++;
            lstf_codenode_unref(lhs_variable);
            return;
        }
        lstf_scope_add_symbol(current_scope, lhs_variable);
        assign->lhs->symbol_reference = lhs_variable;
        lhs_declared = (lstf_variable *)lhs_variable;
        // also resolve the explicit data type, if there is one
        if (assign->lhs->value_type)
            lstf_codenode_accept(assign->lhs->value_type, visitor);
    }
    lstf_codenode_accept(assign->lhs, visitor);

    if (lhs_declared && assign->lhs->value_type) {
        lstf_variable_set_variable_type(lhs_declared, assign->lhs->value_type);
    }
}

static void
lstf_symbolresolver_visit_block(lstf_codevisitor *visitor, lstf_block *block)
{
    lstf_symbolresolver *resolver = (lstf_symbolresolver *)visitor;
    
    ptr_list_append(resolver->scopes, block->scope);
    lstf_codenode_accept_children(block, visitor);
    ptr_list_remove_last_link(resolver->scopes);
}

static void
lstf_symbolresolver_resolve_data_type(lstf_symbolresolver *resolver, lstf_datatype *data_type)
{
    lstf_unresolvedtype *unresolved_type = lstf_unresolvedtype_cast(data_type);

    if (!unresolved_type)
        return;

    lstf_codenode_ref(unresolved_type);

    lstf_datatype *replacement_type = NULL;

    if (strcmp(unresolved_type->name, "integer") == 0) {
        replacement_type = lstf_integertype_new(&((lstf_codenode *)data_type)->source_reference);
    } else if (strcmp(unresolved_type->name, "double") == 0) {
        replacement_type = lstf_doubletype_new(&((lstf_codenode *)data_type)->source_reference);
    } else if (strcmp(unresolved_type->name, "number") == 0) {
        replacement_type = lstf_numbertype_new(&((lstf_codenode *)data_type)->source_reference);
    } else if (strcmp(unresolved_type->name, "boolean") == 0) {
        replacement_type = lstf_booleantype_new(&((lstf_codenode *)data_type)->source_reference);
    } else if (strcmp(unresolved_type->name, "string") == 0) {
        replacement_type = lstf_stringtype_new(&((lstf_codenode *)data_type)->source_reference);
    } else if (strcmp(unresolved_type->name, "object") == 0) {
        replacement_type = lstf_objecttype_new(&((lstf_codenode *)data_type)->source_reference);
    } else if (strcmp(unresolved_type->name, "array") == 0) {
        replacement_type = lstf_arraytype_new(&((lstf_codenode *)data_type)->source_reference,
                lstf_anytype_new(NULL));
    } else if (strcmp(unresolved_type->name, "any") == 0) {
        replacement_type = lstf_anytype_new(&((lstf_codenode *)data_type)->source_reference);
    } else if (strcmp(unresolved_type->name, "pattern") == 0) {
        replacement_type = lstf_patterntype_new(&((lstf_codenode *)data_type)->source_reference);
    } else if (strcmp(unresolved_type->name, "void") == 0) {
        replacement_type = lstf_voidtype_new(&((lstf_codenode *)data_type)->source_reference);
    } else {
        lstf_scope *current_scope = ptr_list_node_get_data(resolver->scopes->tail, lstf_scope *);
        lstf_symbol *found_symbol = NULL;
        lstf_typesymbol *found_type_symbol = NULL;

        if (!(found_symbol = lstf_scope_lookup(current_scope, unresolved_type->name)) ||
                !(found_type_symbol = lstf_typesymbol_cast(found_symbol))) {
            lstf_report_error(&((lstf_codenode *)data_type)->source_reference,
                    "`%s' does not refer to a type", unresolved_type->name);
            resolver->num_errors++;
            lstf_codenode_unref(unresolved_type);
            return;
        }

        switch (found_type_symbol->typesymbol_type) {
        case lstf_typesymbol_type_enum:
            replacement_type = lstf_enumtype_new(&((lstf_codenode *)unresolved_type)->source_reference,
                    (lstf_enum *)found_type_symbol);
            break;
        case lstf_typesymbol_type_interface:
            replacement_type = lstf_interfacetype_new(&((lstf_codenode *)unresolved_type)->source_reference, 
                    (lstf_interface *)found_type_symbol);
            break;
        case lstf_typesymbol_type_alias:
            replacement_type = lstf_datatype_copy(((lstf_typealias *)found_type_symbol)->aliased_type);
            ((lstf_codenode *)replacement_type)->source_reference = ((lstf_codenode *)unresolved_type)->source_reference;
            break;
        }
    }

    // replace the type
    lstf_codenode *parent = ((lstf_codenode *)unresolved_type)->parent_node;

    if (parent->codenode_type == lstf_codenode_type_symbol) {
        lstf_symbol *parent_symbol = (lstf_symbol *)parent;

        if (parent_symbol->symbol_type == lstf_symbol_type_variable) {
            lstf_variable *parent_variable = (lstf_variable *)parent_symbol;

            lstf_variable_set_variable_type(parent_variable, replacement_type);
        } else if (parent_symbol->symbol_type == lstf_symbol_type_function) {
            lstf_function *parent_function = (lstf_function *)parent_symbol;

            lstf_function_set_return_type(parent_function, replacement_type);
        } else if (parent_symbol->symbol_type == lstf_symbol_type_interfaceproperty) {
            lstf_interfaceproperty *parent_property = (lstf_interfaceproperty *)parent_symbol;

            lstf_interfaceproperty_set_property_type(parent_property, replacement_type);
        } else {
            lstf_typesymbol *parent_ts = lstf_typesymbol_cast(parent);
            if (parent_ts && parent_ts->typesymbol_type == lstf_typesymbol_type_alias) {
                lstf_typealias_set_aliased_type(lstf_typealias_cast(parent_ts), replacement_type);
            } else if (parent_ts && parent_ts->typesymbol_type == lstf_typesymbol_type_interface) {
                lstf_interface *parent_interface = lstf_interface_cast(parent_ts);

                lstf_interface_replace_base_type(parent_interface,
                        (lstf_datatype *)unresolved_type, replacement_type);
            } else {
                fprintf(stderr, "%s: bad tree: symbol data type must be child of a variable, function, interface, or interface property\n", __func__);
                abort();
            }
        }
    } else if (parent->codenode_type == lstf_codenode_type_expression) {
        lstf_expression *parent_expression = (lstf_expression *)parent;

        lstf_expression_set_value_type(parent_expression, replacement_type);
    } else if (parent->codenode_type == lstf_codenode_type_datatype) {
        lstf_datatype *parent_dt = lstf_datatype_cast(parent);
        if (parent_dt->datatype_type == lstf_datatype_type_uniontype) {
            lstf_union_type_replace_option(lstf_uniontype_cast(parent_dt), 
                    (lstf_datatype *)unresolved_type, replacement_type);
        } else if (parent_dt->datatype_type == lstf_datatype_type_functiontype) {
            lstf_functiontype *parent_ft = lstf_functiontype_cast(parent_dt);

            if (parent_ft->return_type == (lstf_datatype *)unresolved_type)
                lstf_functiontype_set_return_type(parent_ft, replacement_type);
            else
                lstf_functiontype_replace_parameter_type(parent_ft, 
                        (lstf_datatype *)unresolved_type, replacement_type);
        } else if (parent_dt->datatype_type == lstf_datatype_type_arraytype) {
            lstf_arraytype *parent_at = lstf_arraytype_cast(parent_dt);

            lstf_arraytype_set_element_type(parent_at, replacement_type);
        } else {
            fprintf(stderr, "%s: bad tree: unexpected parent data type for unresolved type\n", __func__); 
            abort();
        }
    } else {
        fprintf(stderr, "%s: bad tree: data type must be child of a symbol, expression, or data type\n", __func__); 
        abort();
    }

    lstf_codenode_unref(unresolved_type);
}

static void
lstf_symbolresolver_visit_data_type(lstf_codevisitor *visitor, lstf_datatype *data_type)
{
    lstf_codenode_accept_children(data_type, visitor);
    lstf_symbolresolver_resolve_data_type((lstf_symbolresolver *)visitor, data_type);
}

static void
lstf_symbolresolver_visit_declaration(lstf_codevisitor *visitor, lstf_declaration *decl)
{
    lstf_codenode_accept_children(decl, visitor);
}

static void
lstf_symbolresolver_visit_element_access(lstf_codevisitor *visitor, lstf_elementaccess *access)
{
    lstf_codenode_accept_children(access, visitor);
}

static void
lstf_symbolresolver_visit_enum(lstf_codevisitor *visitor, lstf_enum *enum_symbol)
{
    lstf_symbolresolver *resolver = (lstf_symbolresolver *)visitor;
    lstf_scope *current_scope = ptr_list_node_get_data(resolver->scopes->tail, lstf_scope *);
    lstf_symbol *clashing_sym = NULL;

    if ((clashing_sym = lstf_scope_lookup(current_scope, lstf_symbol_cast(enum_symbol)->name))) {
        lstf_report_error(&lstf_codenode_cast(enum_symbol)->source_reference,
                "enum declaration conflicts with previous declaration");
        lstf_report_note(&lstf_codenode_cast(clashing_sym)->source_reference,
                "previous declaration was here");
        resolver->num_errors++;
        return;
    }

    lstf_scope_add_symbol(current_scope, lstf_symbol_cast(enum_symbol));
    lstf_codenode_accept_children(enum_symbol, visitor);
}

static void
lstf_symbolresolver_visit_expression(lstf_codevisitor *visitor, lstf_expression *expr)
{
    if (expr->value_type)
        lstf_symbolresolver_resolve_data_type((lstf_symbolresolver *)visitor, expr->value_type);
}

static void
lstf_symbolresolver_visit_expression_statement(lstf_codevisitor *visitor, lstf_expressionstatement *stmt)
{
    lstf_codenode_accept_children(stmt, visitor);
}

static void
lstf_symbolresolver_visit_file(lstf_codevisitor *visitor, lstf_file *file)
{
    // create built-in variables and functions
    lstf_sourceref src = lstf_sourceref_default_from_file(file);
    lstf_symbol *server_path = lstf_variable_new(&src, "server_path", true);
    lstf_variable_set_variable_type(lstf_variable_cast(server_path), lstf_stringtype_new(&src));
    lstf_scope_add_symbol(file->main_block->scope, server_path);

    lstf_symbol *project_files = lstf_variable_new(&src, "project_files", true);
    lstf_variable_set_variable_type(lstf_variable_cast(project_files),
            lstf_arraytype_new(&src, lstf_stringtype_new(&src)));
    lstf_scope_add_symbol(file->main_block->scope, project_files);

    lstf_function *diagnostics = (lstf_function *)
        lstf_function_new(&src, "diagnostics", lstf_anytype_new(&src), false, true, true);
    lstf_variable *diagnostics_args[] = {
        (lstf_variable *)lstf_variable_new(&src, "file", true),
    };
    lstf_variable_set_variable_type(diagnostics_args[0], lstf_stringtype_new(&src));

    for (unsigned i = 0; i < sizeof(diagnostics_args) / sizeof(diagnostics_args[0]); i++)
        lstf_function_add_parameter(diagnostics, diagnostics_args[i]);
    lstf_scope_add_symbol(file->main_block->scope, lstf_symbol_cast(diagnostics));

    lstf_codenode_accept(file->main_block, visitor);
}

static void
lstf_symbolresolver_visit_function(lstf_codevisitor *visitor, lstf_function *function)
{
    lstf_symbolresolver *resolver = (lstf_symbolresolver *)visitor;
    lstf_scope *current_scope = ptr_list_node_get_data(resolver->scopes->tail, lstf_scope *);
    lstf_symbol *clashing_sym = NULL;

    if ((clashing_sym = lstf_scope_lookup(current_scope, lstf_symbol_cast(function)->name))) {
        lstf_report_error(&lstf_codenode_cast(function)->source_reference,
                "function declaration conflicts with previous declaration");
        lstf_report_note(&lstf_codenode_cast(clashing_sym)->source_reference,
                "previous declaration was here");
        resolver->num_errors++;
        return;
    }

    lstf_scope_add_symbol(current_scope, lstf_symbol_cast(function));

    ptr_list_append(resolver->scopes, function->scope);
    lstf_codenode_accept_children(function, visitor);
    ptr_list_remove_last_link(resolver->scopes);
}

static void
lstf_symbolresolver_visit_interface(lstf_codevisitor *visitor, lstf_interface *interface)
{
    lstf_symbolresolver *resolver = (lstf_symbolresolver *)visitor;
    lstf_scope *current_scope = ptr_list_node_get_data(resolver->scopes->tail, lstf_scope *);

    lstf_scope_add_symbol(current_scope, lstf_symbol_cast(interface));
    lstf_codenode_accept_children(interface, visitor);
}

static void
lstf_symbolresolver_visit_interface_property(lstf_codevisitor *visitor, lstf_interfaceproperty *property)
{
    lstf_codenode_accept_children(property, visitor);
}

static void
lstf_symbolresolver_visit_member_access(lstf_codevisitor *visitor, lstf_memberaccess *access)
{
    lstf_symbolresolver *resolver = (lstf_symbolresolver *)visitor;
    lstf_scope *current_scope = ptr_list_node_get_data(resolver->scopes->tail, lstf_scope *);

    lstf_codenode_accept_children(access, visitor);

    lstf_expression *expr = (lstf_expression *)access;
    // Only check trivial member accesses at this stage.
    // Analyzing nontrivial accesses requires type analysis at a later stage.
    if (!access->inner) {
        if (!expr->symbol_reference)
            expr->symbol_reference = lstf_scope_lookup(current_scope, access->member_name);
        if (!expr->symbol_reference) {
            lstf_report_error(&((lstf_codenode *)access)->source_reference,
                    "`%s' undeclared", access->member_name);
            resolver->num_errors++;
            return;
        }
    }
}

static void
lstf_symbolresolver_visit_method_call(lstf_codevisitor *visitor, lstf_methodcall *mcall)
{
    lstf_codenode_accept_children(mcall, visitor);
}

static void
lstf_symbolresolver_visit_object(lstf_codevisitor *visitor, lstf_object *object)
{
    lstf_codenode_accept_children(object, visitor);
}

static void
lstf_symbolresolver_visit_object_property(lstf_codevisitor *visitor, lstf_objectproperty *property)
{
    lstf_codenode_accept_children(property, visitor);
}

static void
lstf_symbolresolver_visit_pattern_test(lstf_codevisitor *visitor, lstf_patterntest *stmt)
{
    lstf_codenode_accept_children(stmt, visitor);
}

static void
lstf_symbolresolver_visit_return_statement(lstf_codevisitor *visitor, lstf_returnstatement *stmt)
{
    lstf_codenode_accept_children(stmt, visitor);
}

struct temp_codevisitor {
    lstf_codevisitor parent_struct;
    lstf_typesymbol *root_symbol;
    lstf_datatype *encountered_unresolved_type;
};

static void temp_codevisitor_visit_data_type(lstf_codevisitor *visitor, lstf_datatype *data_type)
{
    struct temp_codevisitor *tmp_visitor = (struct temp_codevisitor *)visitor;
    if (!tmp_visitor->encountered_unresolved_type &&
            data_type->datatype_type == lstf_datatype_type_unresolvedtype &&
            strcmp(lstf_unresolvedtype_cast(data_type)->name, 
                lstf_symbol_cast(tmp_visitor->root_symbol)->name) == 0)
        tmp_visitor->encountered_unresolved_type = data_type;
    lstf_codenode_accept_children(data_type, visitor);
}

static void
lstf_symbolresolver_visit_type_alias(lstf_codevisitor *visitor, lstf_typealias *alias)
{
    lstf_symbolresolver *resolver = (lstf_symbolresolver *)visitor;
    lstf_scope *current_scope = ptr_list_node_get_data(resolver->scopes->tail, lstf_scope *);

    lstf_scope_add_symbol(current_scope, lstf_symbol_cast(alias));
    lstf_codenode_accept_children(alias, visitor);

    struct temp_codevisitor temp_visitor;
    temp_visitor.root_symbol = lstf_typesymbol_cast(alias);
    temp_visitor.encountered_unresolved_type = NULL;
    static const lstf_codevisitor_vtable temp_visitor_vtable = {
        .visit_data_type = temp_codevisitor_visit_data_type
    };
    lstf_codevisitor_construct((lstf_codevisitor *)&temp_visitor,
            &temp_visitor_vtable);
    lstf_codenode_accept(alias->aliased_type, (lstf_codevisitor *)&temp_visitor);

    if (temp_visitor.encountered_unresolved_type) {
        lstf_report_error(&lstf_codenode_cast(alias)->source_reference,
                "type `%s' circularly references itself",
                lstf_symbol_cast(alias)->name);
        lstf_report_note(&lstf_codenode_cast(temp_visitor.encountered_unresolved_type)->source_reference,
                "circular reference made here");
        resolver->num_errors++;
    }
}

static void
lstf_symbolresolver_visit_variable(lstf_codevisitor *visitor, lstf_variable *variable)
{
    lstf_symbolresolver *resolver = (lstf_symbolresolver *)visitor;
    lstf_function *parent_function = lstf_function_cast(((lstf_codenode *)variable)->parent_node);

    if (parent_function) {
        // then this is a parameter declaration
        lstf_scope *current_scope = ptr_list_node_get_data(resolver->scopes->tail, lstf_scope *);
        lstf_symbol *clashing_param = lstf_scope_get_symbol(current_scope, ((lstf_symbol *)variable)->name);

        if (clashing_param) {
            lstf_report_error(&((lstf_codenode *)variable)->source_reference,
                    "duplicate parameter `%s'",
                    ((lstf_symbol *)variable)->name);
            lstf_report_note(&((lstf_codenode *)clashing_param)->source_reference,
                    "previous declaration of parameter `%s' was here",
                    clashing_param->name);
            resolver->num_errors++;
            return;
        }
        lstf_scope_add_symbol(current_scope, (lstf_symbol *)variable);
    }

    lstf_codenode_accept_children(variable, visitor);
}

static const lstf_codevisitor_vtable symbolresolver_vtable = {
    lstf_symbolresolver_visit_array,
    lstf_symbolresolver_visit_assignment,
    lstf_symbolresolver_visit_block,
    NULL,
    lstf_symbolresolver_visit_data_type,
    lstf_symbolresolver_visit_declaration,
    lstf_symbolresolver_visit_element_access,
    NULL,
    lstf_symbolresolver_visit_enum,
    lstf_symbolresolver_visit_expression,
    lstf_symbolresolver_visit_expression_statement,
    lstf_symbolresolver_visit_file,
    lstf_symbolresolver_visit_function,
    lstf_symbolresolver_visit_interface,
    lstf_symbolresolver_visit_interface_property,
    NULL,
    lstf_symbolresolver_visit_member_access,
    lstf_symbolresolver_visit_method_call,
    lstf_symbolresolver_visit_object,
    lstf_symbolresolver_visit_object_property,
    lstf_symbolresolver_visit_pattern_test,
    lstf_symbolresolver_visit_return_statement,
    lstf_symbolresolver_visit_type_alias,
    lstf_symbolresolver_visit_variable
};

lstf_symbolresolver *lstf_symbolresolver_new(lstf_file *file)
{
    lstf_symbolresolver *resolver = calloc(1, sizeof *resolver);

    lstf_codevisitor_construct((lstf_codevisitor *)resolver, &symbolresolver_vtable);
    resolver->file = file;
    resolver->scopes = ptr_list_new((collection_item_ref_func) lstf_codenode_ref, 
            (collection_item_unref_func) lstf_codenode_unref);
    resolver->expected_element_type = NULL;

    return resolver;
}

void lstf_symbolresolver_resolve(lstf_symbolresolver *resolver)
{
    lstf_codevisitor_visit_file((lstf_codevisitor *)resolver, resolver->file);
}

void lstf_symbolresolver_destroy(lstf_symbolresolver *resolver)
{
    ptr_list_destroy(resolver->scopes);
    resolver->scopes = NULL;
    resolver->file = NULL;
    free(resolver);
}
