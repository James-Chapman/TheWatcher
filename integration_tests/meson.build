server_agent_integration_test = executable(
  'server_agent_integration_test',
  ['server_agent_integration_test.cpp'],
  include_directories: root_inc,
  dependencies: [
    agent_dep,
    server_dep,
    common_dep,
    catch2_dep,
    http_dep,
    cppzmq_dep,
    sodium_dep,
    cbor_dep,
    json_dep,
    platform_deps,
  ],
)
test('server_agent_integration', server_agent_integration_test, timeout: 60)
