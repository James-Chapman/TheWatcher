common_sources = []

if host_machine.system() == 'windows'
  common_sources += ['windows_service.cpp']
endif

# Mirrors Bazel's :common cc_library. nlohmann_json comes in via collector_config.hpp's
# REST-side macros (kept by user decision: REST stays JSON).
common_lib = static_library(
  'watcher_common',
  common_sources,
  include_directories: root_inc,
  dependencies: [
    cbor_dep,
    sodium_dep,
    json_dep,
    platform_deps,
  ],
)

common_dep = declare_dependency(
  link_with: common_lib,
  include_directories: root_inc,
  dependencies: [
    cbor_dep,
    sodium_dep,
    json_dep,
    platform_deps,
  ],
)
