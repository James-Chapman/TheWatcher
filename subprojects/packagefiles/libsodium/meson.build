project(
  'libsodium',
  'c',
  version: '1.0.20',
  default_options: [
    'default_library=static',
    'warning_level=0',
  ],
)

cc = meson.get_compiler('c')
python = import('python').find_installation()

source_list = run_command(
  python,
  files('list_sources.py'),
  meson.current_source_dir(),
  check: true,
).stdout().strip().split('\n')

run_command(
  python,
  files('copy_headers.py'),
  meson.current_source_dir() / 'src' / 'libsodium' / 'include',
  meson.current_build_dir(),
  check: true,
)

libsodium_inc = include_directories(
  '.',
  'src/libsodium/include',
  'src/libsodium/include/sodium',
)

libsodium_c_args = [
  '-DCONFIGURED=1',
  '-DNATIVE_LITTLE_ENDIAN',
  '-DSODIUM_STATIC',
]

if cc.get_id() == 'msvc'
  libsodium_c_args += [
    '/D_CRT_SECURE_NO_WARNINGS',
    '/Dinline=__inline',
    '/wd4146',
    '/wd4244',
  ]
else
  libsodium_c_args += ['-D_GNU_SOURCE']
endif

libsodium_lib = static_library(
  'sodium',
  source_list,
  include_directories: libsodium_inc,
  c_args: libsodium_c_args,
  pic: true,
  install: false,
)

libsodium_dep = declare_dependency(
  link_with: libsodium_lib,
  include_directories: libsodium_inc,
  compile_args: ['-DSODIUM_STATIC'],
)

libsodium_include_paths = [
  meson.current_build_dir(),
]
libsodium_library_path = libsodium_lib.full_path()

meson.override_dependency('libsodium', libsodium_dep)
meson.override_dependency('sodium', libsodium_dep)
