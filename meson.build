project('lstf', 'c',
  version: '0.0.1',
  license: 'GPLv3+',
  default_options: ['c_std=c11', 'buildtype=debugoptimized'])

cc = meson.get_compiler('c')

add_project_arguments([
  '-I' + meson.source_root() + '/src',
  '-D_XOPEN_SOURCE=700',
], language: 'c')

c_args = cc.get_supported_arguments([
  '-Werror',
  '-Wall',
  '-Wextra',
  '-Wpedantic',
  '-Wshadow',
  '-Wno-cast-function-type',
  '-Wformat=2',
  '-Wformat-truncation=2',
  '-Wformat-overflow=2',
  '-Wnull-dereference',
  '-Wstrict-aliasing=3',
  '-Wcast-align=strict',
  '-Wcast-qual',
  '-Wuninitialized',
])

subdir('src')
subdir('tests')
