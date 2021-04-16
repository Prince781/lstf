jsonrpc_notification = executable('jsonrpc-notification',
  dependencies: [jsonrpc],
  c_args: c_args,
  sources: ['jsonrpc-notification.c'],
  install: false,
)

test('notification', jsonrpc_notification, suite: 'jsonrpc',
  args: [meson.project_source_root() + '/tests/jsonrpc/notification.json'])

jsonrpc_call = executable('jsonrpc-call',
  dependencies: [jsonrpc],
  c_args: c_args,
  sources: ['jsonrpc-call.c'],
  install: false,
)

test('call', jsonrpc_call, suite: 'jsonrpc',
  args: [meson.project_source_root() + '/tests/jsonrpc/call.json'])

jsonrpc_batched = executable('jsonrpc-batched',
  dependencies: [jsonrpc],
  c_args: c_args,
  sources: ['jsonrpc-batched.c'],
  install: false,
)

test('batched', jsonrpc_batched, suite: 'jsonrpc',
  args: [meson.project_source_root() + '/tests/jsonrpc/batched.json'])
