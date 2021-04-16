json_serialize_test = executable('json-serialize-test',
  dependencies: [json],
  c_args: c_args,
  sources: ['json-serialize-test.c'],
  install: false,
)

test('serialize', json_serialize_test, suite: 'json',
  args: [meson.project_source_root() + '/tests/json/serialize.json'])
test('serialize-pretty', json_serialize_test, suite: 'json',
  args: [meson.project_source_root() + '/tests/json/serialize-pretty.json', '--pretty-print'])

json_cycle_serialize_test = executable('json-cycle-serialize-test',
  dependencies: [json],
  c_args: c_args,
  sources: ['json-cycle-serialize-test.c'],
  install: false,
)

test('cycle-serialize-pretty', json_cycle_serialize_test, suite: 'json',
  args: [meson.project_source_root() + '/tests/json/cycle-serialize-pretty.txt', '--pretty-print'])

json_parser_object = executable('json-parser-object',
  dependencies: [json],
  c_args: c_args,
  sources: ['json-parser-object.c'],
  install: false,
)

test('parse-object', json_parser_object, suite: 'json',
  args: [meson.project_source_root() + '/tests/json/parse-object.json'])

json_parser_double = executable('json-parser-double',
  dependencies: [json],
  c_args: c_args,
  sources: ['json-parser-double.c'],
  install: false,
)

test('parse-double', json_parser_double, suite: 'json',
  args: [meson.project_source_root() + '/tests/json/parse-double.json'])

json_parser_memory = executable('json-parser-memory',
  dependencies: [json],
  c_args: c_args,
  sources: ['json-parser-memory.c'],
  install: false,
)

test('parse-in-memory-string', json_parser_memory, suite: 'json')

json_pattern_array = executable('json-pattern-array',
  dependencies: [json],
  c_args: c_args,
  sources: ['json-pattern-array.c'],
  install: false,
)

test('pattern-array', json_pattern_array, suite: 'json')

json_pattern_object = executable('json-pattern-object',
  dependencies: [json],
  c_args: c_args,
  sources: ['json-pattern-object.c'],
  install: false,
)

test('pattern-object', json_pattern_object, suite: 'json')

json_pattern_optional_member = executable('json-pattern-optional-member',
  dependencies: [json],
  c_args: c_args,
  sources: ['json-pattern-optional-member.c'],
  install: false,
)

test('pattern-optional-member', json_pattern_optional_member, suite: 'json')
