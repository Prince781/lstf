json_serialize_test = executable('json-serialize-test',
  dependencies: [jsonrpc],
  c_args: c_args,
  sources: ['json-serialize-test.c'],
  install: false,
)

test('serialize', json_serialize_test, suite: 'json',
  args: [meson.source_root() + '/tests/json/serialize.json'])
test('serialize-pretty', json_serialize_test, suite: 'json',
  args: [meson.source_root() + '/tests/json/serialize-pretty.json', '--pretty-print'])

json_parser_object = executable('json-parser-object',
  dependencies: [jsonrpc],
  c_args: c_args,
  sources: ['json-parser-object.c'],
  install: false,
)

test('parse-object', json_parser_object, suite: 'json',
  args: [meson.source_root() + '/tests/json/parse-object.json'])

json_parser_double = executable('json-parser-double',
  dependencies: [jsonrpc],
  c_args: c_args,
  sources: ['json-parser-double.c'],
  install: false,
)

test('parse-double', json_parser_double, suite: 'json',
  args: [meson.source_root() + '/tests/json/parse-double.json'])
