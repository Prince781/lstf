module.exports = grammar({
    name: 'lstf',
    
    word: $ => $.identifier,

    rules: {
        assignment_statement: $ => seq(choice($.member_access_expression, $.element_access_expression), '=', $.expression, ';'),

        // taken from tree-sitter-c
        // http://stackoverflow.com/questions/13014947/regex-to-match-a-c-style-multiline-comment/36328890#36328890
        comment: $ => token(prec(1, choice(
          seq('//', /(\\(.|\r?\n)|[^\\\n])*/),
          seq(
            '/*',
            /[^*]*\*+([^/*][^*]*\*+)*/,
            '/'
          )
        ))),

        data_type: $ => seq($.simple_data_type, repeat(seq('|', $.simple_data_type))),

        declaration_statement: $ => choice($.function_declaration, $.variable_declaration),

        expression_statement: $ => seq($.statement_expression, ';'),

        function_data_type: $ => seq(optional('async'), '(', optional(seq($.parameter_declaration, repeat(',', $.parameter_declaration))), ')', '=>', $.data_type),

        function_declaration: $ => seq(optional('async'), 'fun', $.identifier, '(', optional(seq($.parameter_declaration, repeat(',', $.parameter_declaration))), ')', ':', $.data_type, '{', $.statement_list, '}'),

        identifier: $ => /[@A-Za-z_]\w*/,

        parameter_declaration: $ => seq($.identifier, ':', $.data_type),

        simple_data_type: $ => choice(seq($.element_data_type, repeat(seq('[', ']'))), $.function_data_type),

        source_file: $ => $.statement_list,

        statement: $ => choice($.assignment_statement, $.declaration_statement, $.expression_statement),

        statement_list: $ => repeat($.statement),

        statement_expression: $ => choice($.method_call_expression, $.await_expression),

        variable_declaration: $ => seq('let', $.identifier, optional(seq(':', $.data_type)), '=', $.expression, ';'),

        // --- expressions ---
        expression: $ => choice($.lambda_expression, $.value_expression),
        lambda_parameter_declaration: $ => seq($.identifier, optional(seq(':', $.data_type))),
        lambda_expression: $ => seq(optional('async'), '(', optional(seq($.lambda_parameter_declaration, repeat(seq(',', $.lambda_parameter_declaration)))), ')', '=>', choice(seq('{', $.statement_list, '}'), $.expression)),
        value_expression: $ => choice(
            $.conditional_expression,
            $.coalescing_expression,
            $.conditional_or_expression,
            // TODO: more ...
        ),
        conditional_expression: $ => prec.right(1, $.value_expression, '?', $.value_expression, ':', $.value_expression),
        coalescing_expression: $ => prec.left(2, $.value_expression, '??', $.value_expression),
        conditional_or_expression: $ => prec.left(3, $.value_expression, '||', $.value_expression),
        conditional_and_expression: $ => prec.left(4, $.value_expression, '&&', $.value_expression),
        in_expression: $ => prec.left(5, $.value_expression, 'in', $.value_expression),
        inclusive_or_expression: $ => prec.left(6, $.value_expression, '|', $.value_expression),
        exclusive_or_expression: $ => prec.left(7, $.value_expression, '^', $.value_expression),
        and_expression: $ => prec.left(8, $.value_expression, '&', $.value_expression),
        equality_expression: $ => prec.left(9, $.value_expression, choice('==', '!=', '<=>'), $.value_expression),
        relational_expression: $ => prec.left(10, $.value_expression, choice('<', '<=', '>', '>='), $.value_expression),
        shift_expression: $ => prec.left(11, $.value_expression, choice('<<', '>>'), $.value_expression),
        additive_expression: $ => prec.left(12, $.value_expression, choice('+', '-'), $.value_expression),
        multiplicative_expression: $ => prec.left(13, $.value_expression, choice('*', '/', '%'), $.value_expression),
        unary_expression: $ => prec.right(14, choice('-', '!', '~'), $.value_expression),

        // TODO: more expressions in this list
        primary_expression: $ => choice($.simple_name, $.literal_expression, $.method_call_expression, $.element_access_expression, seq('(', $.expression, ')')),
    }
});
