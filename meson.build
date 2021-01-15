project('lstf', 'c',
  version: '0.0.1',
  license: 'GPLv3+',
  default_options: ['c_std=c11', 'buildtype=debugoptimized', 'werror=true'])

cc = meson.get_compiler('c')

add_project_arguments([
  '-I' + meson.source_root() + '/src',
  '-D_XOPEN_SOURCE=700',
], language: 'c')

if cc.get_id() == 'msvc'
    c_args = cc.get_supported_arguments([
        '/D_CRT_SECURE_NO_WARNINGS',
        '/wd4996',      # suppress POSIX name deprecation warnings
        '/wd4706',      # suppress assignment within conditional expression
    ])
else
    c_args = cc.get_supported_arguments([
      '-Wextra',
      '-Wpedantic',
      '-pedantic-errors',
      '-Warray-bounds=2',
      '-Wcast-qual',
      '-Wshadow',
      '-Wno-cast-function-type',
      '-Wformat=2',
      '-Wformat-overflow=2',
      '-Wformat-signedness',
      '-Wformat-truncation=2',
      #      '-Wswitch-enum',
      '-Wunreachable-code'
    ])
endif

subdir('src')
subdir('tests')
