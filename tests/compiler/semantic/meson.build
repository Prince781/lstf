semantic_test_args = ['-no-lsp', '--disable=codegen']

test('semantic-assignments-typed', lstf, suite: 'compiler',
  args: semantic_test_args + [meson.source_root() + '/tests/compiler/semantic/assignments-typed.lstf'])

test('semantic-closure-access', lstf, suite: 'compiler',
  args: semantic_test_args + [meson.source_root() + '/tests/compiler/semantic/closure-access.lstf'])

test('semantic-function-call', lstf, suite: 'compiler',
  args: semantic_test_args + [meson.source_root() + '/tests/compiler/semantic/function-call.lstf'])

test('semantic-function-types', lstf, suite: 'compiler',
  args: semantic_test_args + [meson.source_root() + '/tests/compiler/semantic/function-types.lstf'])

test('semantic-insufficient-arguments', lstf, suite: 'compiler', should_fail: true,
  args: semantic_test_args + [meson.source_root() + '/tests/compiler/semantic/insufficient-arguments.lstf'])

test('semantic-invalid-access', lstf, suite: 'compiler', should_fail: true,
  args: semantic_test_args + [meson.source_root() + '/tests/compiler/semantic/invalid-access.lstf'])

test('semantic-invalid-assignments', lstf, suite: 'compiler', should_fail: true,
  args: semantic_test_args + [meson.source_root() + '/tests/compiler/semantic/invalid-assignments.lstf'])

test('semantic-not-callable', lstf, suite: 'compiler', should_fail: true,
  args: semantic_test_args + [meson.source_root() + '/tests/compiler/semantic/not-callable.lstf'])

test('semantic-object-element-write', lstf, suite: 'compiler',
  args: semantic_test_args + [meson.source_root() + '/tests/compiler/semantic/object-element-write.lstf'])

test('semantic-object-element-access', lstf, suite: 'compiler',
  args: semantic_test_args + [meson.source_root() + '/tests/compiler/semantic/object-element-access.lstf'])

test('semantic-object-member-write', lstf, suite: 'compiler',
  args: semantic_test_args + [meson.source_root() + '/tests/compiler/semantic/object-member-write.lstf'])

test('semantic-lambda-as-argument', lstf, suite: 'compiler',
  args: semantic_test_args + [meson.source_root() + '/tests/compiler/semantic/passed-lambda-with-expression-body.lstf'])

test('semantic-required-await', lstf, suite: 'compiler', should_fail: true,
  args: semantic_test_args + [meson.source_root() + '/tests/compiler/semantic/required-await-in-async.lstf'])

test('semantic-string-element-access', lstf, suite: 'compiler',
  args: semantic_test_args + [meson.source_root() + '/tests/compiler/semantic/string-element-access.lstf'])

test('semantic-too-many-arguments', lstf, suite: 'compiler', should_fail: true,
  args: semantic_test_args + [meson.source_root() + '/tests/compiler/semantic/too-many-arguments.lstf'])
