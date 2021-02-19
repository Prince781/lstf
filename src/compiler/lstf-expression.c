#include "lstf-expression.h"
#include "lstf-array.h"
#include "lstf-literal.h"
#include "lstf-object.h"
#include "lstf-assignment.h"
#include "lstf-datatype.h"
#include "lstf-codenode.h"
#include <stdlib.h>
#include <stdio.h>

void lstf_expression_construct(lstf_expression            *expr,
                               const lstf_codenode_vtable *vtable,
                               const lstf_sourceref       *source_reference,
                               lstf_expression_type        expr_type)
{
    lstf_codenode_construct((lstf_codenode *)expr,
            vtable,
            lstf_codenode_type_expression, 
            source_reference);
    expr->expr_type = expr_type;
}

void lstf_expression_destruct(lstf_codenode *node)
{
    lstf_codenode_unref(((lstf_expression *)node)->value_type);
}

void lstf_expression_set_value_type(lstf_expression *expression, lstf_datatype *data_type)
{
    lstf_codenode_unref(expression->value_type);

    if (((lstf_codenode *)data_type)->parent_node)
        data_type = lstf_datatype_copy(data_type);
    expression->value_type = lstf_codenode_ref(data_type);

    lstf_codenode_set_parent(expression->value_type, expression);
}

bool lstf_expression_is_lvalue(lstf_expression *expression)
{
    lstf_assignment *assign = lstf_assignment_cast(lstf_codenode_cast(expression)->parent_node);

    if (assign)
        return assign->lhs == expression;

    return false;
}

json_node *lstf_expression_to_json(lstf_expression *expression)
{
    json_node *node = NULL;

    switch (expression->expr_type) {
        case lstf_expression_type_array:
            node = lstf_array_cast(expression)->is_pattern ? json_array_pattern_new() : json_array_new();

            for (iterator it = ptr_list_iterator_create(lstf_array_cast(expression)->expression_list);
                    it.has_next; it = iterator_next(it)) {
                lstf_expression *element = iterator_get_item(it);
                json_node *element_json = lstf_expression_to_json(element);

                if (!element_json) {
                    json_node_unref(node);
                    return NULL;
                }

                json_array_add_element(node, element_json);
            }
            break;
        case lstf_expression_type_binary:
        case lstf_expression_type_conditional:
        case lstf_expression_type_elementaccess:
            break;
        case lstf_expression_type_ellipsis:
            node = json_ellipsis_new();
            break;
        case lstf_expression_type_lambda:
            break;
        case lstf_expression_type_literal:
            switch (lstf_literal_cast(expression)->literal_type) {
                case lstf_literal_type_boolean:
                    node = json_boolean_new(lstf_literal_cast(expression)->value.boolean_value);
                    break;
                case lstf_literal_type_double:
                    node = json_double_new(lstf_literal_cast(expression)->value.double_value);
                    break;
                case lstf_literal_type_integer:
                    node = json_integer_new(lstf_literal_cast(expression)->value.integer_value);
                    break;
                case lstf_literal_type_null:
                    node = json_null_new();
                    break;
                case lstf_literal_type_string:
                    node = json_string_new(lstf_literal_cast(expression)->value.string_value);
                    break;
            }
            break;
        case lstf_expression_type_memberaccess:
        case lstf_expression_type_methodcall:
            break;
        case lstf_expression_type_object:
            node = lstf_object_cast(expression)->is_pattern ? json_object_pattern_new() : json_object_new();

            for (iterator it = ptr_list_iterator_create(lstf_object_cast(expression)->members_list);
                    it.has_next; it = iterator_next(it)) {
                lstf_objectproperty *property = iterator_get_item(it);
                json_node *property_value_json = lstf_expression_to_json(property->value);

                if (!property_value_json) {
                    json_node_unref(node);
                    return NULL;
                }

                property_value_json->optional = property->is_nullable;
                json_object_set_member(node, lstf_symbol_cast(property)->name, property_value_json);
            }
            break;
        case lstf_expression_type_unary:
            break;
    }

    return node;
}
