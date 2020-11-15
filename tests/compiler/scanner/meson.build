lstf_scanner_test = executable('lstf-scanner-test',
  dependencies: [compiler],
  c_args: c_args,
  sources: ['lstf-scanner-test.c'],
  install: false,
)

test('scanner-double', lstf_scanner_test, suite: 'compiler',
  args: [meson.source_root() + '/tests/compiler/scanner/double.lstf'])

test('scanner-example', lstf_scanner_test, suite: 'compiler',
  args: [meson.source_root() + '/tests/compiler/scanner/example.lstf'])

test('scanner-malformed-comments', lstf_scanner_test, suite: 'compiler', should_fail: true,
  args: [meson.source_root() + '/tests/compiler/scanner/malformed-comments.lstf'])

test('scanner-unterminated-string', lstf_scanner_test, suite: 'compiler', should_fail: true,
  args: [meson.source_root() + '/tests/compiler/scanner/malformed-comments.lstf'])
