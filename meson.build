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
      '-pedantic-errors',
      '-Wall',
      '-Wextra',
      '-Wpedantic',
      '-Warray-bounds=2',
      '-Wcast-align',         # supported by Clang and GCC (but not enough for GCC)
      '-Wcast-align=strict',  # needed by GCC, unsupported by Clang
      '-Wcast-qual',
      '-Wno-cast-function-type',
      '-Wduplicated-branches',
      '-Wduplicated-cond',
      '-Wformat=2',
      '-Wformat-nonliteral',
      '-Wformat-overflow=2',
      '-Wformat-security',
      '-Wformat-signedness',
      '-Wformat-truncation=2',
      '-Wimplicit-fallthrough',
      '-Wlogical-op',
      '-Wmissing-include-dirs',
      '-Wmissing-declarations',
      '-Wshadow',
      '-Wstrict-overflow',
      '-Wsuggest-attribute=malloc',
      '-Wtautological-compare',
      '-Wundef',
      '-Wuninitialized',
      '-Wunreachable-code',
      '-Wunused',
    ])
endif

subdir('src')
subdir('tests')
