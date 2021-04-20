parser_test_args = ['--disable=resolver']

test('parser-array', lstf, suite: 'compiler',
  args: parser_test_args + [meson.project_source_root() + '/tests/compiler/parser/array.lstf'])

test('parser-class', lstf, suite: 'compiler', should_fail: true,
  args: parser_test_args + [meson.project_source_root() + '/tests/compiler/parser/class-decl.lstf'])

test('parser-const', lstf, suite: 'compiler', should_fail: true,
  args: parser_test_args + [meson.project_source_root() + '/tests/compiler/parser/const-decl.lstf'])

test('parser-if-statements', lstf, suite: 'compiler',
  args: parser_test_args + [meson.project_source_root() + '/tests/compiler/parser/if-statements.lstf'])

test('parser-object', lstf, suite: 'compiler',
  args: parser_test_args + [meson.project_source_root() + '/tests/compiler/parser/object.lstf'])

test('parser-pattern-test', lstf, suite: 'compiler',
  args: parser_test_args + [meson.project_source_root() + '/tests/compiler/parser/pattern-test.lstf'])

test('parser-unterminated-statements', lstf, suite: 'compiler', should_fail: true,
  args: parser_test_args + [meson.project_source_root() + '/tests/compiler/parser/unterminated-statements.lstf'])

