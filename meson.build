project('lstf', 'c',
  version: '0.0.1',
  license: 'GPLv3+',
  default_options: ['c_std=c11'])

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
# '-Wconversion',
  '-Wno-cast-function-type',
  '-Wformat=2',
  '-Wformat-truncation=2',
  '-Wnull-dereference',
  '-fdelete-null-pointer-checks',
])

subdir('src')
subdir('tests')
