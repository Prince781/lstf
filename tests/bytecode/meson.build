lstf_bc_serialize_test = executable('lstf-bc-serialize-test',
  dependencies: [bytecode, io],
  c_args: c_args,
  sources: ['lstf-bc-serialize-test.c'],
  install: false,
)

test('serialize-hello-world', lstf_bc_serialize_test, suite: 'bytecode')
