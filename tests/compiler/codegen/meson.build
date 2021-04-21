test('codegen-array', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/array.lstf',
    '-expect', '[\n    [\n        1,\n        2,\n        3\n    ],\n    {\n        "obj": {\n            "hello": true,\n            "goodbye": false\n        }\n    },\n    3,\n    4,\n    "string",\n    3.141590\n]\n'])

test('codegen-closure', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/closure.lstf',
    '-expect', 'outer\ndoughnut\nbagel\ndoughnut\nbagel\n'])

test('codegen-factorial', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/factorial.lstf',
    '-expect', 'factorial(10) = ...\n3628800\n'])

test('codegen-if-statements', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/if-statements.lstf',
    '-expect', 'x is less than y\nmidpoint\nx is less than y\n'])

test('codegen-object', lstf, suite: 'compiler',
  args: ['-no-lsp', meson.project_source_root() + '/tests/compiler/codegen/object.lstf',
    '-expect', '{\n    "prop1": false,\n    "prop2": "hello",\n    "prop3": {\n        "prop1": false,\n        "prop2": 3.141590\n    },\n    "prop4": 3.141590,\n    "prop5": []\n}\n'])
