memory_pipe = executable('memory-pipe',
  dependencies: [io],
  include_directories: include_dirs,
  c_args: c_args,
  sources: ['memory-pipe.c'],
  install: false,
)

test('memory-pipe', memory_pipe, suite: 'io')

event_pipe = executable('event-pipe',
  dependencies: [io],
  include_directories: include_dirs,
  c_args: c_args,
  sources: ['event-pipe.c'],
  install: false
)

test('event-pipe', event_pipe, timeout: 2, suite: 'io')
