lsp_server = executable('lsp-server',
  dependencies: [lsp, jsonrpc],
  include_directories: include_dirs,
  c_args: c_args,
  sources: ['lsp-server.c'],
  install: false,
)

test('server', lsp_server, suite: 'lsp',
  args: [meson.project_source_root() + '/tests/lsp/stream.txt'])
