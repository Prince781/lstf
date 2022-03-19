test('codegen-array', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/array.lstf',
    '-expect', '[\n    [\n        1,\n        2,\n        3\n    ],\n    {\n        "obj": {\n            "hello": true,\n            "goodbye": false\n        }\n    },\n    3,\n    4,\n    "string",\n    3.141590\n]\n'])

test('codegen-assert', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/assert.lstf',
    '-expect', 'success\n'])

test('codegen-async-lambda', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/async-lambda.lstf',
    '-expect', '42\n'])

test('codegen-async-no-await', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/async-no-await.lstf',
    '-expect', '42\n'])

test('codegen-closure', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/closure.lstf',
    '-expect', 'outer\ndoughnut\nbagel\ndoughnut\nbagel\n'])

test('codegen-closure-modified', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/closure-modified.lstf',
    '-expect', '42\n'])

test('codegen-factorial', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/factorial.lstf',
    '-expect', 'factorial(10) = ...\n3628800\n'])

test('codegen-if-statements', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/if-statements.lstf',
    '-expect', 'x is less than y\nmidpoint\nx is less than y\n'])

test('codegen-lambda', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/lambda.lstf',
    '-expect', '42\n'])

test('codegen-object-nonconst', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/object-nonconst.lstf',
    '-expect', '{\n    "prop": 3\n}\n'])

test('codegen-object', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/object.lstf',
    '-expect', '{\n    "prop1": false,\n    "prop2": "hello",\n    "prop3": {\n        "prop1": false,\n        "prop2": 3.141590\n    },\n    "prop4": 3.141590,\n    "prop5": []\n}\n'])

test('codegen-verbatim-string', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/verbatim-string.lstf', '-expect', 'a\nb\na\\nb\n'])
