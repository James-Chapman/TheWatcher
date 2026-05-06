project(
  'thewatcher',
  ['c', 'cpp'],
  version: '0.4.0',
  default_options: [
    'cpp_std=c++20',
    'default_library=static',
    'prefer_static=true',
    'warning_level=2',
    'force_fallback_for=libcbor,sqlite3,nlohmann_json,cpp-httplib,catch2-with-main,cppzmq',
  ],
)

cpp = meson.get_compiler('cpp')
root_inc = include_directories('.')

add_project_arguments(
  '-DCBOR_BUILD_STATIC=1',
  language: ['c', 'cpp'],
)

if host_machine.system() == 'windows'
  # Match Bazel's effective Windows defaults: prevent <windows.h> from
  # defining min/max as macros, target Win10+ APIs.
  add_project_arguments(
    '-DNOMINMAX',
    '-D_WIN32_WINNT=0x0A00',
    '-DWIN32_LEAN_AND_MEAN',
    language: ['c', 'cpp'],
  )
endif

thread_dep = dependency('threads')

platform_deps = [thread_dep]

if host_machine.system() == 'windows'
  platform_deps += declare_dependency(link_args: [
    'ws2_32.lib',
    'iphlpapi.lib',
    'shell32.lib',
    'advapi32.lib',
    'pdh.lib',
    'psapi.lib',
  ])
endif

# Local static deps: subprojects/<name>.wrap downloads source archives and builds
# the libraries in this build tree. System/vcpkg packages are not used.
cbor_dep   = dependency('libcbor',          fallback: ['libcbor',       'libcbor_dep'])
sqlite_dep = dependency('sqlite3',          fallback: ['sqlite3',       'sqlite3_dep'])
json_dep   = dependency('nlohmann_json',    fallback: ['nlohmann_json', 'nlohmann_json_dep'])
http_dep   = dependency('cpp-httplib',      fallback: ['cpp-httplib',   'cpp_httplib_dep'])
catch2_dep = dependency('catch2-with-main', fallback: ['catch2',        'catch2_with_main_dep'])

libsodium_project = subproject(
  'libsodium',
  default_options: ['default_library=static'],
)
sodium_dep = libsodium_project.get_variable('libsodium_dep')
libsodium_cmake_include_path = meson.current_build_dir() / 'subprojects' / 'zeromq-4.3.5' / 'libsodium-include'

python = import('python').find_installation()
run_command(
  python,
  files('scripts/copy_headers.py'),
  meson.current_build_dir() / 'subprojects' / 'libsodium-1.0.20',
  libsodium_cmake_include_path,
  check: true,
)

cmake = import('cmake')
libzmq_options = cmake.subproject_options()
libzmq_options.add_cmake_defines({
  'BUILD_SHARED': 'OFF',
  'BUILD_STATIC': 'ON',
  'BUILD_TESTS': 'OFF',
  'ENABLE_CURVE': 'ON',
  'WITH_LIBSODIUM': 'ON',
  'WITH_LIBSODIUM_STATIC': 'ON',
  'CMAKE_POSITION_INDEPENDENT_CODE': 'ON',
  'CMAKE_POLICY_VERSION_MINIMUM': '3.5',
  'ENABLE_PRECOMPILED': 'OFF',
  'SODIUM_INCLUDE_DIRS': libsodium_cmake_include_path,
  'SODIUM_LIBRARIES': libsodium_project.get_variable('libsodium_library_path'),
})
libzmq_project = cmake.subproject('libzmq', options: libzmq_options)
zmq_dep = declare_dependency(
  dependencies: [
    libzmq_project.dependency('libzmq-static'),
    sodium_dep,
  ],
)
meson.override_dependency('libzmq', zmq_dep)
meson.override_dependency('zeromq', zmq_dep)

cppzmq_dep = dependency('cppzmq', fallback: ['cppzmq', 'cppzmq_dep'])

subdir('common')
subdir('agent/collectors')
subdir('agent')
subdir('server')

subdir('common_tests')
subdir('agent_tests')
subdir('server_tests')
subdir('integration_tests')
