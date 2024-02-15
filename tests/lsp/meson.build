lsp_server = executable('lsp-server',
  dependencies: [lsp, jsonrpc],
  include_directories: include_dirs,
  c_args: c_args,
  sources: ['lsp-server.c'],
  install: false,
)

lsp_server_input = custom_target('lsp-server-input',
  output: 'stream.txt',
  input: [meson.project_source_root() + '/tests/lsp/stream.json'],
  command: [prepend_content_length, '@INPUT@', '@OUTPUT@']
)

test('server', lsp_server, suite: 'lsp', args: [lsp_server_input])
