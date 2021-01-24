#pragma once

#include "lstf-conditionalexpression.h"
#include "lstf-lambdaexpression.h"
#include "lstf-unaryexpression.h"
#include "lstf-binaryexpression.h"
#include "lstf-returnstatement.h"
#include "lstf-typealias.h"
#include "lstf-declaration.h"
#include "lstf-interface.h"
#include "lstf-constant.h"
#include "lstf-enum.h"
#include "lstf-datatype.h"
#include "lstf-expressionstatement.h"
#include "lstf-patterntest.h"
#include "lstf-function.h"
#include "lstf-variable.h"
#include "lstf-statement.h"
#include "lstf-symbol.h"
#include "lstf-memberaccess.h"
#include "lstf-methodcall.h"
#include "lstf-object.h"
#include "lstf-elementaccess.h"
#include "lstf-ellipsis.h"
#include "lstf-expression.h"
#include "lstf-literal.h"
#include "lstf-file.h"
#include "lstf-assignment.h"
#include "lstf-array.h"

typedef struct _lstf_codevisitor lstf_codevisitor;

struct _lstf_codevisitor_vtable {
    /**
     * Visits an array element
     */
    void (*visit_array)(lstf_codevisitor *code_visitor, lstf_array *array);

    /**
     * Visits an assignment
     */
    void (*visit_assignment)(lstf_codevisitor *code_visitor, lstf_assignment *assign);

    /**
     * Visits a binary expression
     */
    void (*visit_binary_expression)(lstf_codevisitor *code_visitor, lstf_binaryexpression *expr);

    /**
     * Visits a block
     */
    void (*visit_block)(lstf_codevisitor *code_visitor, lstf_block *block);

    /**
     * Visits a conditional expression
     */
    void (*visit_conditional_expression)(lstf_codevisitor *code_visitor, lstf_conditionalexpression *expr);

    /**
     * Visits a constant
     */
    void (*visit_constant)(lstf_codevisitor *code_visitor, lstf_constant *constant);

    /**
     * Visits a data type
     */
    void (*visit_data_type)(lstf_codevisitor *code_visitor, lstf_datatype *data_type);

    /**
     * Visits a declaration statement
     */
    void (*visit_declaration)(lstf_codevisitor *code_visitor, lstf_declaration *declaration);

    /**
     * Visits an element access expression
     */
    void (*visit_element_access)(lstf_codevisitor *code_visitor, lstf_elementaccess *access);

    /**
     * Visits an ellipsis expression
     */
    void (*visit_ellipsis)(lstf_codevisitor *code_visitor, lstf_ellipsis *ellipsis);

    /**
     * Visits an enum declaration
     */
    void (*visit_enum)(lstf_codevisitor *code_visitor, lstf_enum *enum_symbol);

    /**
     * Visits any kind of expression
     */
    void (*visit_expression)(lstf_codevisitor *code_visitor, lstf_expression *expr);

    /**
     * Visits an expression statement
     */
    void (*visit_expression_statement)(lstf_codevisitor *code_visitor, lstf_expressionstatement *stmt);

    /**
     * Visits a LSTF source file
     */
    void (*visit_file)(lstf_codevisitor *code_visitor, lstf_file *file);

    /**
     * Visits a LSTF function
     */
    void (*visit_function)(lstf_codevisitor *code_visitor, lstf_function *function);

    /**
     * Visits an interface
     */
    void (*visit_interface)(lstf_codevisitor *code_visitor, lstf_interface *interface);

    /**
     * Visits an interface property
     */
    void (*visit_interface_property)(lstf_codevisitor *code_visitor, lstf_interfaceproperty *property);

    /**
     * Visits a lambda expression
     */
    void (*visit_lambda_expression)(lstf_codevisitor *code_visitor, lstf_lambdaexpression *expr);

    /**
     * Visits a literal expression
     */
    void (*visit_literal)(lstf_codevisitor *code_visitor, lstf_literal *lit);

    /**
     * Visits a member access expression
     */
    void (*visit_member_access)(lstf_codevisitor *code_visitor, lstf_memberaccess *access);

    /**
     * Visits a method call expression
     */
    void (*visit_method_call)(lstf_codevisitor *code_visitor, lstf_methodcall *mcall);

    /**
     * Visits an object expression
     */
    void (*visit_object)(lstf_codevisitor *code_visitor, lstf_object *object);

    /**
     * Visits an object property
     */
    void (*visit_object_property)(lstf_codevisitor *code_visitor, lstf_objectproperty *property);

    /**
     * Visits a pattern test statement
     */
    void (*visit_pattern_test)(lstf_codevisitor *code_visitor, lstf_patterntest *stmt);

    /**
     * Visits a return statement
     */
    void (*visit_return_statement)(lstf_codevisitor *code_visitor, lstf_returnstatement *stmt);

    /**
     * Visits a type alias
     */
    void (*visit_type_alias)(lstf_codevisitor *code_visitor, lstf_typealias *alias);

    /**
     * Visits a unary expression
     */
    void (*visit_unary_expression)(lstf_codevisitor *code_visitor, lstf_unaryexpression *expr);

    /**
     * Visits a variable
     */
    void (*visit_variable)(lstf_codevisitor *code_visitor, lstf_variable *variable);
};
typedef struct _lstf_codevisitor_vtable lstf_codevisitor_vtable;

struct _lstf_codevisitor {
    const lstf_codevisitor_vtable *vtable;
};

void lstf_codevisitor_construct(lstf_codevisitor *code_visitor, const lstf_codevisitor_vtable *vtable);

void lstf_codevisitor_visit_array(lstf_codevisitor *code_visitor, lstf_array *array);

void lstf_codevisitor_visit_assignment(lstf_codevisitor *code_visitor, lstf_assignment *assign);

void lstf_codevisitor_visit_binary_expression(lstf_codevisitor *code_visitor, lstf_binaryexpression *expr);

void lstf_codevisitor_visit_block(lstf_codevisitor *code_visitor, lstf_block *block);

void lstf_codevisitor_visit_conditional_expression(lstf_codevisitor *code_visitor, lstf_conditionalexpression *expr);

void lstf_codevisitor_visit_constant(lstf_codevisitor *code_visitor, lstf_constant *constant);

void lstf_codevisitor_visit_data_type(lstf_codevisitor *code_visitor, lstf_datatype *data_type);

void lstf_codevisitor_visit_declaration(lstf_codevisitor *code_visitor, lstf_declaration *decl);

void lstf_codevisitor_visit_element_access(lstf_codevisitor *code_visitor, lstf_elementaccess *access);

void lstf_codevisitor_visit_ellipsis(lstf_codevisitor *code_visitor, lstf_ellipsis *ellipsis);

void lstf_codevisitor_visit_enum(lstf_codevisitor *code_visitor, lstf_enum *enum_symbol);

void lstf_codevisitor_visit_expression(lstf_codevisitor *code_visitor, lstf_expression *expr);

void lstf_codevisitor_visit_expression_statement(lstf_codevisitor *code_visitor, lstf_expressionstatement *stmt);

void lstf_codevisitor_visit_file(lstf_codevisitor *code_visitor, lstf_file *file);

void lstf_codevisitor_visit_function(lstf_codevisitor *code_visitor, lstf_function *function);

void lstf_codevisitor_visit_interface(lstf_codevisitor *code_visitor, lstf_interface *interface);

void lstf_codevisitor_visit_interface_property(lstf_codevisitor *code_visitor, lstf_interfaceproperty *property);

void lstf_codevisitor_visit_lambda_expression(lstf_codevisitor *code_visitor, lstf_lambdaexpression *expr);

void lstf_codevisitor_visit_literal(lstf_codevisitor *code_visitor, lstf_literal *lit);

void lstf_codevisitor_visit_member_access(lstf_codevisitor *code_visitor, lstf_memberaccess *access);

void lstf_codevisitor_visit_method_call(lstf_codevisitor *code_visitor, lstf_methodcall *mcall);

void lstf_codevisitor_visit_object(lstf_codevisitor *code_visitor, lstf_object *object);

void lstf_codevisitor_visit_object_property(lstf_codevisitor *code_visitor, lstf_objectproperty *property);

void lstf_codevisitor_visit_pattern_test(lstf_codevisitor *code_visitor, lstf_patterntest *stmt);

void lstf_codevisitor_visit_return_statement(lstf_codevisitor *code_visitor, lstf_returnstatement *stmt);

void lstf_codevisitor_visit_type_alias(lstf_codevisitor *code_visitor, lstf_typealias *alias);

void lstf_codevisitor_visit_unary_expression(lstf_codevisitor *code_visitor, lstf_unaryexpression *expr);

void lstf_codevisitor_visit_variable(lstf_codevisitor *code_visitor, lstf_variable *variable);
