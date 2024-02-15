jsonrpc_notification = executable('jsonrpc-notification',
  dependencies: [jsonrpc],
  include_directories: include_dirs,
  c_args: c_args,
  sources: ['jsonrpc-notification.c'],
  install: false,
)

jsonrpc_notification_input = custom_target('jsonrpc-notification-input',
  output: 'notification.txt',
  input: [meson.project_source_root() + '/tests/jsonrpc/notification.json'],
  command: [prepend_content_length, '@INPUT@', '@OUTPUT@']
)

test('notification', jsonrpc_notification, suite: 'jsonrpc',
  args: [jsonrpc_notification_input])

jsonrpc_call = executable('jsonrpc-call',
  dependencies: [jsonrpc],
  include_directories: include_dirs,
  c_args: c_args,
  sources: ['jsonrpc-call.c'],
  install: false,
)

jsonrpc_call_input = custom_target('jsonrpc-call-input',
  output: 'call.txt',
  input: [meson.project_source_root() + '/tests/jsonrpc/call.json'],
  command: [prepend_content_length, '@INPUT@', '@OUTPUT@']
)

test('call', jsonrpc_call, suite: 'jsonrpc',
  args: [jsonrpc_call_input])

jsonrpc_batched = executable('jsonrpc-batched',
  dependencies: [jsonrpc],
  include_directories: include_dirs,
  c_args: c_args,
  sources: ['jsonrpc-batched.c'],
  install: false,
)

jsonrpc_batched_input = custom_target('jsonrpc-batched-input',
  output: 'batched.txt',
  input: [meson.project_source_root() + '/tests/jsonrpc/batched.json'],
  command: [prepend_content_length, '@INPUT@', '@OUTPUT@']
)

test('batched', jsonrpc_batched, suite: 'jsonrpc',
  args: [jsonrpc_batched_input])
