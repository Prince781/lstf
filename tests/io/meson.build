io_memory_pipe = executable('io-memory-pipe',
  dependencies: [io],
  include_directories: include_dirs,
  c_args: c_args,
  sources: ['io-memory-pipe.c'],
  install: false,
)

io_file_pipe = executable('io-file-pipe',
  dependencies: [io],
  include_directories: include_dirs,
  c_args: c_args,
  sources: ['io-file-pipe.c'],
  install: false,
)

test('io-memory-pipe', io_memory_pipe, suite: 'io')
test('io-file-pipe', io_file_pipe, suite: 'io')
