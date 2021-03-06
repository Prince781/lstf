project('lstf', 'c',
  version: '0.0.1',
  license: 'GPLv3+',
  default_options: ['c_std=c17', 'buildtype=debugoptimized', 'werror=true'])

cc = meson.get_compiler('c')

# MSVC supports both '-' and '/' in front of a command line flag
add_project_arguments([
  '-I' + meson.project_source_root() + '/src',
], language: 'c')

if host_machine.system() == 'windows'
  add_project_arguments([
    '-DWIN32_LEAN_AND_MEAN',  # Win32 headers don't pull in more than they need
    '-D_WIN32_WINNT=0x0601',
    '-D_CRT_SECURE_NO_WARNINGS',
  ], language: 'c')
else
  add_project_arguments([
    '-D_XOPEN_SOURCE=700',
  ], language: 'c')
endif

if cc.get_id() == 'msvc'
    c_args = cc.get_supported_arguments([
      '/wd4996',      # suppress POSIX name deprecation warnings
      '/wd4706',      # suppress assignment within conditional expression
      '/wd5105',      # suppress warning about undefined 'defined' macro usage outside of #if
      '/std:c17',
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
      '-Wstringop-overflow',
      '-Wsuggest-attribute=malloc',
      '-Wtautological-compare',
      '-Wundef',
      '-Wuninitialized',
      '-Wunreachable-code',
      '-Wunused',
      '-Wunused-function',    # needed by Clang
      '-Wvla',
    ])
endif

if not cc.has_header('stdatomic.h')
    if cc.has_argument('-isystem')
        add_project_arguments([
            '-isystem' + meson.project_source_root() + '/include/c11/atomic'
        ], language: 'c')
    else
        add_project_arguments([
            '-I' + meson.project_source_root() + '/include/c11/atomic'
        ], language: 'c')
    endif
endif

if not cc.has_header('threads.h')
    if cc.has_argument('-isystem')
        add_project_arguments([
            '-isystem' + meson.project_source_root() + '/include/c11/threads'
        ], language: 'c')
    else
        add_project_arguments([
            '-I' + meson.project_source_root() + '/include/c11/threads'
        ], language: 'c')
    endif
endif

if not cc.has_header('stdalign.h')
    if cc.has_argument('-isystem')
        add_project_arguments([
            '-isystem' + meson.project_source_root() + '/include/c11/align'
        ], language: 'c')
    else
        add_project_arguments([
            '-I' + meson.project_source_root() + '/include/c11/align'
        ], language: 'c')
    endif
endif

subdir('src')
subdir('tests')
