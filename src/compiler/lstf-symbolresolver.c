#include "compiler/lstf-codevisitor.h"
#include "compiler/lstf-expression.h"
#include "compiler/lstf-patterntest.h"
#include "compiler/lstf-statement.h"
#include "compiler/lstf-variable.h"
#include "lstf-methodcall.h"
#include "lstf-object.h"
#include "lstf-expressionstatement.h"
#include "lstf-function.h"
#include "data-structures/ptr-list.h"
#include "lstf-symbolresolver.h"
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
#include <stdlib.h>

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

    if (assign->is_declaration) {
        assert(assign->lhs->expr_type == lstf_expression_type_memberaccess &&
                ((lstf_memberaccess *)assign->lhs)->inner == NULL &&
                "`let' assignment must have identifier after keyword");

        lstf_symbol *lhs_variable = lstf_variable_new(&((lstf_codenode *)assign->lhs)->source_reference, 
                ((lstf_memberaccess *)assign->lhs)->member_name);

        lstf_symbol *clashing_var = NULL;
        if ((clashing_var = lstf_scope_get_symbol(current_scope, lhs_variable->name))) {
            lstf_report_error(&((lstf_codenode *)lhs_variable)->source_reference, 
                    "redefinition of `%s'", lhs_variable->name);
            lstf_report_note(&((lstf_codenode *)clashing_var)->source_reference,
                    "previous definition of `%s' was here", lhs_variable->name);
            resolver->num_errors++;
            lstf_codenode_unref(lhs_variable);
            return;
        }
        lstf_scope_add_symbol(current_scope, lhs_variable);
        assign->lhs->symbol_reference = lhs_variable;
    } else {
        lstf_codenode_accept(assign->lhs, visitor);
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
lstf_symbolresolver_visit_element_access(lstf_codevisitor *visitor, lstf_elementaccess *access)
{
    lstf_codenode_accept_children(access, visitor);
}

static void
lstf_symbolresolver_visit_expression_statement(lstf_codevisitor *visitor, lstf_expressionstatement *stmt)
{
    lstf_codenode_accept_children(stmt, visitor);
}

static void
lstf_symbolresolver_visit_file(lstf_codevisitor *visitor, lstf_file *file)
{
    lstf_symbolresolver *resolver = (lstf_symbolresolver *)visitor;

    ptr_list_append(resolver->scopes, file->main_block->scope);
    lstf_codenode_accept(file->main_block, visitor);
    ptr_list_remove_last_link(resolver->scopes);
}

static void
lstf_symbolresolver_visit_function(lstf_codevisitor *visitor, lstf_function *function)
{
    lstf_codenode_accept_children(function, visitor);
}

static void
lstf_symbolresolver_visit_member_access(lstf_codevisitor *visitor, lstf_memberaccess *access)
{
    lstf_symbolresolver *resolver = (lstf_symbolresolver *)visitor;
    lstf_scope *current_scope = ptr_list_node_get_data(resolver->scopes->tail, lstf_scope *);

    lstf_codenode_accept_children(access, visitor);

    lstf_expression *expr = (lstf_expression *)access;
    if (!access->inner) {
        expr->symbol_reference = lstf_scope_get_symbol(current_scope, access->member_name);
        if (!expr->symbol_reference) {
            lstf_report_error(&((lstf_codenode *)access)->source_reference,
                    "`%s' does not refer to a variable, function, or type",
                    access->member_name);
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
lstf_symbolresolver_visit_statement(lstf_codevisitor *visitor, lstf_statement *stmt)
{
    lstf_codenode_accept_children(stmt, visitor);
}

static const lstf_codevisitor_vtable symbolresolver_vtable = {
    lstf_symbolresolver_visit_array,
    lstf_symbolresolver_visit_assignment,
    lstf_symbolresolver_visit_block,
    lstf_symbolresolver_visit_element_access,
    NULL,
    NULL,
    lstf_symbolresolver_visit_expression_statement,
    lstf_symbolresolver_visit_file,
    lstf_symbolresolver_visit_function,
    NULL,
    lstf_symbolresolver_visit_member_access,
    lstf_symbolresolver_visit_method_call,
    lstf_symbolresolver_visit_object,
    lstf_symbolresolver_visit_object_property,
    lstf_symbolresolver_visit_pattern_test,
    lstf_symbolresolver_visit_statement,
    NULL
};

lstf_symbolresolver *lstf_symbolresolver_new(lstf_file *file)
{
    lstf_symbolresolver *resolver = calloc(1, sizeof *resolver);

    lstf_codevisitor_construct((lstf_codevisitor *)resolver, &symbolresolver_vtable);
    resolver->file = file;
    resolver->scopes = ptr_list_new((collection_item_ref_func) lstf_codenode_ref, 
            (collection_item_unref_func) lstf_codenode_unref);

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
