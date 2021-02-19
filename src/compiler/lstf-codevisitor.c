#include "lstf-codevisitor.h"
#include <assert.h>

void lstf_codevisitor_construct(lstf_codevisitor *code_visitor, const lstf_codevisitor_vtable *vtable)
{
    // codevisitors can be created without malloc(), so we have to initialize
    // everything here
    code_visitor->vtable = vtable;
}

void lstf_codevisitor_visit_array(lstf_codevisitor *code_visitor, lstf_array *array)
{
    if (code_visitor->vtable->visit_array)
        code_visitor->vtable->visit_array(code_visitor, array);
}

void lstf_codevisitor_visit_assignment(lstf_codevisitor *code_visitor, lstf_assignment *assign)
{
    if (code_visitor->vtable->visit_assignment)
        code_visitor->vtable->visit_assignment(code_visitor, assign);
}

void lstf_codevisitor_visit_binary_expression(lstf_codevisitor *code_visitor, lstf_binaryexpression *expr)
{
    if (code_visitor->vtable->visit_binary_expression)
        code_visitor->vtable->visit_binary_expression(code_visitor, expr);
}

void lstf_codevisitor_visit_block(lstf_codevisitor *code_visitor, lstf_block *block)
{
    if (code_visitor->vtable->visit_block)
        code_visitor->vtable->visit_block(code_visitor, block);
}

void lstf_codevisitor_visit_conditional_expression(lstf_codevisitor *code_visitor, lstf_conditionalexpression *expr)
{
    if (code_visitor->vtable->visit_conditional_expression)
        code_visitor->vtable->visit_conditional_expression(code_visitor, expr);
}

void lstf_codevisitor_visit_constant(lstf_codevisitor *code_visitor, lstf_constant *constant)
{
    if (code_visitor->vtable->visit_constant)
        code_visitor->vtable->visit_constant(code_visitor, constant);
}

void lstf_codevisitor_visit_data_type(lstf_codevisitor *code_visitor, lstf_datatype *data_type)
{
    if (code_visitor->vtable->visit_data_type)
        code_visitor->vtable->visit_data_type(code_visitor, data_type);
}

void lstf_codevisitor_visit_declaration(lstf_codevisitor *code_visitor, lstf_declaration *decl)
{
    if (code_visitor->vtable->visit_declaration)
        code_visitor->vtable->visit_declaration(code_visitor, decl);
}

void lstf_codevisitor_visit_element_access(lstf_codevisitor *code_visitor, lstf_elementaccess *access)
{
    if (code_visitor->vtable->visit_element_access)
        code_visitor->vtable->visit_element_access(code_visitor, access);
}

void lstf_codevisitor_visit_ellipsis(lstf_codevisitor *code_visitor, lstf_ellipsis *ellipsis)
{
    if (code_visitor->vtable->visit_ellipsis)
        code_visitor->vtable->visit_ellipsis(code_visitor, ellipsis);
}

void lstf_codevisitor_visit_enum(lstf_codevisitor *code_visitor, lstf_enum *enum_symbol)
{
    if (code_visitor->vtable->visit_enum)
        code_visitor->vtable->visit_enum(code_visitor, enum_symbol);
}

void lstf_codevisitor_visit_expression(lstf_codevisitor *code_visitor, lstf_expression *expr)
{
    if (code_visitor->vtable->visit_expression)
        code_visitor->vtable->visit_expression(code_visitor, expr);
}

void lstf_codevisitor_visit_expression_statement(lstf_codevisitor *code_visitor, lstf_expressionstatement *stmt)
{
    if (code_visitor->vtable->visit_expression_statement)
        code_visitor->vtable->visit_expression_statement(code_visitor, stmt);
}

void lstf_codevisitor_visit_file(lstf_codevisitor *code_visitor, lstf_file *file)
{
    if (code_visitor->vtable->visit_file)
        code_visitor->vtable->visit_file(code_visitor, file);
}

void lstf_codevisitor_visit_function(lstf_codevisitor *code_visitor, lstf_function *function)
{
    if (code_visitor->vtable->visit_function)
        code_visitor->vtable->visit_function(code_visitor, function);
}

void lstf_codevisitor_visit_if_statement(lstf_codevisitor *code_visitor, lstf_ifstatement *stmt)
{
    if (code_visitor->vtable->visit_if_statement)
        code_visitor->vtable->visit_if_statement(code_visitor, stmt);
}

void lstf_codevisitor_visit_interface(lstf_codevisitor *code_visitor, lstf_interface *interface)
{
    if (code_visitor->vtable->visit_interface)
        code_visitor->vtable->visit_interface(code_visitor, interface);
}

void lstf_codevisitor_visit_interface_property(lstf_codevisitor *code_visitor, lstf_interfaceproperty *property)
{
    if (code_visitor->vtable->visit_interface_property)
        code_visitor->vtable->visit_interface_property(code_visitor, property);
}

void lstf_codevisitor_visit_lambda_expression(lstf_codevisitor *code_visitor, lstf_lambdaexpression *expr)
{
    if (code_visitor->vtable->visit_lambda_expression)
        code_visitor->vtable->visit_lambda_expression(code_visitor, expr);
}

void lstf_codevisitor_visit_literal(lstf_codevisitor *code_visitor, lstf_literal *lit)
{
    if (code_visitor->vtable->visit_literal)
        code_visitor->vtable->visit_literal(code_visitor, lit);
}

void lstf_codevisitor_visit_member_access(lstf_codevisitor *code_visitor, lstf_memberaccess *access)
{
    if (code_visitor->vtable->visit_member_access)
        code_visitor->vtable->visit_member_access(code_visitor, access);
}

void lstf_codevisitor_visit_method_call(lstf_codevisitor *code_visitor, lstf_methodcall *mcall)
{
    if (code_visitor->vtable->visit_method_call)
        code_visitor->vtable->visit_method_call(code_visitor, mcall);
}

void lstf_codevisitor_visit_object(lstf_codevisitor *code_visitor, lstf_object *object)
{
    if (code_visitor->vtable->visit_object)
        code_visitor->vtable->visit_object(code_visitor, object);
}

void lstf_codevisitor_visit_object_property(lstf_codevisitor *code_visitor, lstf_objectproperty *property)
{
    if (code_visitor->vtable->visit_object_property)
        code_visitor->vtable->visit_object_property(code_visitor, property);
}

void lstf_codevisitor_visit_pattern_test(lstf_codevisitor *code_visitor, lstf_patterntest *stmt)
{
    if (code_visitor->vtable->visit_pattern_test)
        code_visitor->vtable->visit_pattern_test(code_visitor, stmt);
}

void lstf_codevisitor_visit_return_statement(lstf_codevisitor *code_visitor, lstf_returnstatement *stmt)
{
    if (code_visitor->vtable->visit_return_statement)
        code_visitor->vtable->visit_return_statement(code_visitor, stmt);
}

void lstf_codevisitor_visit_type_alias(lstf_codevisitor *code_visitor, lstf_typealias *alias)
{
    if (code_visitor->vtable->visit_type_alias)
        code_visitor->vtable->visit_type_alias(code_visitor, alias);
}

void lstf_codevisitor_visit_unary_expression(lstf_codevisitor *code_visitor, lstf_unaryexpression *expr)
{
    if (code_visitor->vtable->visit_unary_expression)
        code_visitor->vtable->visit_unary_expression(code_visitor, expr);
}

void lstf_codevisitor_visit_variable(lstf_codevisitor *code_visitor, lstf_variable *variable)
{
    if (code_visitor->vtable->visit_variable)
        code_visitor->vtable->visit_variable(code_visitor, variable);
}
