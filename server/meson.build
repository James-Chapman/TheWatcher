server_config_lib = static_library(
  'watcher_server_config',
  ['config.cpp'],
  include_directories: root_inc,
  dependencies: [
    common_dep,
    sodium_dep,
    cbor_dep,
    platform_deps,
  ],
)

server_config_dep = declare_dependency(
  link_with: server_config_lib,
  include_directories: root_inc,
  dependencies: [
    common_dep,
    sodium_dep,
    cbor_dep,
    platform_deps,
  ],
)

server_store_lib = static_library(
  'watcher_server_store',
  [
    'status_engine.cpp',
    'store_sqlite.cpp',
  ],
  include_directories: root_inc,
  dependencies: [
    common_dep,
    sqlite_dep,
    cbor_dep,
    sodium_dep,
    http_dep,
    json_dep,
    platform_deps,
  ],
)

server_store_dep = declare_dependency(
  link_with: server_store_lib,
  include_directories: root_inc,
  dependencies: [
    common_dep,
    sqlite_dep,
    cbor_dep,
    sodium_dep,
    http_dep,
    json_dep,
    platform_deps,
  ],
)

server_lib = static_library(
  'watcher_server_lib',
  [
    'api.cpp',
    'report_generator.cpp',
    'server.cpp',
    'zap_handler.cpp',
  ],
  include_directories: root_inc,
  dependencies: [
    server_config_dep,
    server_store_dep,
    common_dep,
    cppzmq_dep,
    http_dep,
    sodium_dep,
    json_dep,
    cbor_dep,
    sqlite_dep,
    platform_deps,
  ],
)

server_dep = declare_dependency(
  link_with: server_lib,
  include_directories: root_inc,
  dependencies: [
    server_config_dep,
    server_store_dep,
    common_dep,
    cppzmq_dep,
    http_dep,
    sodium_dep,
    json_dep,
    cbor_dep,
    sqlite_dep,
    platform_deps,
  ],
)

executable(
  'TheWatcherServer',
  'main.cpp',
  dependencies: server_dep,
  install: true,
)
