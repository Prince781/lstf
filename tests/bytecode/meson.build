lstf_bc_serialize_test = executable('lstf-bc-serialize-test',
  dependencies: [bytecode, io],
  include_directories: include_dirs,
  c_args: c_args,
  sources: ['lstf-bc-serialize-test.c'],
  install: false,
)

test('hello-world', lstf_bc_serialize_test, suite: 'bytecode')
