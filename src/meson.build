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
    'data-structures/iterator.c',
    'data-structures/ptr-hashmap.c',
    'data-structures/ptr-list.c',
    'data-structures/string-builder.c'
  ],
  c_args: c_args,
)

data_structures = declare_dependency(
  link_with: [data_structures_lib],
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
  dependencies: [util, data_structures],
  link_with: [json_lib],
)

jsonrpc_lib = static_library('jsonrpc',
  [
    'jsonrpc/jsonrpc-server.c'
  ],
  c_args: c_args,
)

jsonrpc = declare_dependency(
  dependencies: [util, data_structures, json],
  link_with: [jsonrpc_lib],
)

compiler_lib = static_library('compiler',
  [
    'compiler/lstf-anytype.c',
    'compiler/lstf-array.c',
    'compiler/lstf-arraytype.c',
    'compiler/lstf-assignment.c',
    'compiler/lstf-block.c',
    'compiler/lstf-booleantype.c',
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
    'compiler/lstf-integertype.c',
    'compiler/lstf-interface.c',
    'compiler/lstf-interfacetype.c',
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
    'compiler/lstf-uniontype.c',
    'compiler/lstf-unresolvedtype.c',
    'compiler/lstf-variable.c',
    'compiler/lstf-voidtype.c'
  ],
  c_args: c_args,
)

compiler = declare_dependency(
  dependencies: [data_structures, util, json, jsonrpc],
  link_with: [compiler_lib],
)

lstf = executable('lstf',
  dependencies: [compiler],
  c_args: c_args,
  sources: ['main.c'],
  install: true)
