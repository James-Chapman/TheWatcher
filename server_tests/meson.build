server_config_test = executable(
  'server_config_test',
  ['config_test.cpp'],
  include_directories: root_inc,
  dependencies: [server_config_dep, catch2_dep, cbor_dep, platform_deps],
)
test('server_config', server_config_test)

store_test = executable(
  'store_test',
  ['store_test.cpp'],
  include_directories: root_inc,
  dependencies: [server_store_dep, catch2_dep, sqlite_dep, platform_deps],
)
test('store', store_test)

status_engine_test = executable(
  'status_engine_test',
  ['status_engine_test.cpp'],
  include_directories: root_inc,
  dependencies: [server_store_dep, catch2_dep, cbor_dep, sqlite_dep, platform_deps],
)
test('status_engine', status_engine_test)

webhook_security_test = executable(
  'webhook_security_test',
  ['webhook_security_test.cpp'],
  include_directories: root_inc,
  dependencies: [server_store_dep, catch2_dep, platform_deps],
)
test('webhook_security', webhook_security_test)

zap_handler_test = executable(
  'zap_handler_test',
  ['zap_handler_test.cpp'],
  include_directories: root_inc,
  dependencies: [server_dep, common_dep, catch2_dep, cppzmq_dep, sodium_dep, platform_deps],
)
test('zap_handler', zap_handler_test)

server_lifecycle_test = executable(
  'server_lifecycle_test',
  ['server_lifecycle_test.cpp'],
  include_directories: root_inc,
  dependencies: [server_dep, common_dep, catch2_dep, cppzmq_dep, sqlite_dep, platform_deps],
)
test('server_lifecycle', server_lifecycle_test)

if libpq_dep.found()
  store_postgres_test = executable(
    'store_postgres_test',
    ['store_postgres_test.cpp'],
    cpp_args: ['-DHAVE_LIBPQ'],
    include_directories: root_inc,
    dependencies: [server_store_dep, catch2_dep, platform_deps],
  )
  test(
    'store_postgres',
    store_postgres_test,
    env: ['THEWATCHER_POSTGRES_TEST_DSN=host=localhost dbname=thewatcher_test user=thewatcher_test password=thewatcher_test'],
  )
endif
