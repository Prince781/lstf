util_lib = static_library('util',
  ['util.c'],
  c_args: c_args,
)

util = declare_dependency(
  link_with: [util_lib],
)

data_structures_lib = static_library('data-structures',
  [
    'data-structures/closure.c',
    'data-structures/ptr-hashmap.c',
    'data-structures/ptr-hashset.c',
    'data-structures/ptr-list.c',
    'data-structures/string-builder.c'
  ],
  c_args: c_args,
)

data_structures = declare_dependency(
  link_with: [data_structures_lib],
)

io_lib = static_library('io',
  [
    'io/event.c',
    'io/inputstream.c',
    'io/io-common.c',
    'io/outputstream.c'
  ],
  c_args: c_args,
)

io = declare_dependency(
  dependencies: [data_structures, dependency('threads')],
  link_with: [io_lib],
)

json_lib = static_library('json',
  [
    'json/json-parser.c',
    'json/json-scanner.c',
    'json/json.c'
  ],
  c_args: c_args,
)

json = declare_dependency(
  dependencies: [util, data_structures, io],
  link_with: [json_lib],
)

jsonrpc_lib = static_library('jsonrpc',
  [
    'jsonrpc/jsonrpc-server.c'
  ],
  c_args: c_args,
)

jsonrpc = declare_dependency(
  dependencies: [util, data_structures, json, io],
  link_with: [jsonrpc_lib],
)

compiler_lib = static_library('compiler',
  [
    'compiler/lstf-anytype.c',
    'compiler/lstf-array.c',
    'compiler/lstf-arraytype.c',
    'compiler/lstf-assignment.c',
    'compiler/lstf-binaryexpression.c',
    'compiler/lstf-block.c',
    'compiler/lstf-booleantype.c',
    'compiler/lstf-conditionalexpression.c',
    'compiler/lstf-codegenerator.c',
    'compiler/lstf-codenode.c',
    'compiler/lstf-codevisitor.c',
    'compiler/lstf-constant.c',
    'compiler/lstf-datatype.c',
    'compiler/lstf-declaration.c',
    'compiler/lstf-doubletype.c',
    'compiler/lstf-elementaccess.c',
    'compiler/lstf-ellipsis.c',
    'compiler/lstf-enum.c',
    'compiler/lstf-enumtype.c',
    'compiler/lstf-expression.c',
    'compiler/lstf-expressionstatement.c',
    'compiler/lstf-file.c',
    'compiler/lstf-function.c',
    'compiler/lstf-functiontype.c',
    'compiler/lstf-ifstatement.c',
    'compiler/lstf-integertype.c',
    'compiler/lstf-interface.c',
    'compiler/lstf-interfacetype.c',
    'compiler/lstf-ir-basicblock.c',
    'compiler/lstf-ir-function.c',
    'compiler/lstf-ir-instruction.c',
    'compiler/lstf-ir-node.c',
    'compiler/lstf-ir-program.c',
    'compiler/lstf-lambdaexpression.c',
    'compiler/lstf-literal.c',
    'compiler/lstf-memberaccess.c',
    'compiler/lstf-methodcall.c',
    'compiler/lstf-numbertype.c',
    'compiler/lstf-nulltype.c',
    'compiler/lstf-object.c',
    'compiler/lstf-objecttype.c',
    'compiler/lstf-parser.c',
    'compiler/lstf-parsererror.c',
    'compiler/lstf-patterntest.c',
    'compiler/lstf-patterntype.c',
    'compiler/lstf-report.c',
    'compiler/lstf-returnstatement.c',
    'compiler/lstf-scanner.c',
    'compiler/lstf-scope.c',
    'compiler/lstf-semanticanalyzer.c',
    'compiler/lstf-sourceref.c',
    'compiler/lstf-statement.c',
    'compiler/lstf-stringtype.c',
    'compiler/lstf-symbol.c',
    'compiler/lstf-symbolresolver.c',
    'compiler/lstf-typealias.c',
    'compiler/lstf-typesymbol.c',
    'compiler/lstf-unaryexpression.c',
    'compiler/lstf-uniontype.c',
    'compiler/lstf-unresolvedtype.c',
    'compiler/lstf-variable.c',
    'compiler/lstf-voidtype.c'
  ],
  c_args: c_args,
)

compiler = declare_dependency(
  dependencies: [data_structures, util, json, jsonrpc, io],
  link_with: [compiler_lib],
)

vm_lib = static_library('vm',
  [
    'vm/lstf-virtualmachine.c',
    'vm/lstf-vm-coroutine.c',
    'vm/lstf-vm-loader.c',
    'vm/lstf-vm-program.c',
    'vm/lstf-vm-stack.c',
    'vm/lstf-vm-value.c',
  ],
  c_args: c_args
)

vm = declare_dependency(
  dependencies: [data_structures, util, json, jsonrpc, io, cc.find_library('m', required: false)],
  link_with: [vm_lib]
)

bytecode_lib = static_library('bytecode',
  [
    'bytecode/lstf-bc-function.c',
    'bytecode/lstf-bc-program.c',
    'bytecode/lstf-bc-serialize.c',
  ],
  c_args: c_args
)

bytecode = declare_dependency(
  dependencies: [data_structures, util, json, io, vm],
  link_with: [bytecode_lib]
)

lstf = executable('lstf',
  dependencies: [compiler, vm, bytecode],
  c_args: c_args,
  sources: ['lstf.c'],
  install: true)
