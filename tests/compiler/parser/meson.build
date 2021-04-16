lstf_parser_test = executable('lstf-parser-test',
  dependencies: [compiler],
  c_args: c_args,
  sources: ['lstf-parser-test.c'],
  install: false,
)

test('parser-array', lstf_parser_test, suite: 'compiler',
  args: [meson.project_source_root() + '/tests/compiler/parser/array.lstf'])

test('parser-if-statements', lstf_parser_test, suite: 'compiler',
  args: [meson.project_source_root() + '/tests/compiler/parser/if-statements.lstf'])

test('parser-object', lstf_parser_test, suite: 'compiler',
  args: [meson.project_source_root() + '/tests/compiler/parser/object.lstf'])

test('parser-pattern-test', lstf_parser_test, suite: 'compiler',
  args: [meson.project_source_root() + '/tests/compiler/parser/pattern-test.lstf'])

test('parser-unterminated-statements', lstf_parser_test, suite: 'compiler', should_fail: true,
  args: [meson.project_source_root() + '/tests/compiler/parser/unterminated-statements.lstf'])

