test('hello-world.lstfc', lstf, suite: 'vm',
  args: [meson.source_root() + '/tests/vm/hello-world.lstfc'])

lstf_vm_factorial_test = executable('vm-factorial-test',
  dependencies: [bytecode, vm, io],
  c_args: c_args,
  sources: ['vm-factorial-test.c'],
  install: false
)

test('factorial', lstf_vm_factorial_test, suite: 'vm')
