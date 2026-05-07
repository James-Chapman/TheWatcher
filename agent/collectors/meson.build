collectors_lib = static_library(
  'watcher_collectors',
  [
    'cpu_collector.cpp',
    'disk_collector.cpp',
    'log_collector.cpp',
    'memory_collector.cpp',
    'network_collector.cpp',
    'process_collector.cpp',
    'temperature_collector.cpp',
  ],
  include_directories : root_inc,
  dependencies : [
    common_dep,
    platform_deps,
  ],
)

collectors_dep = declare_dependency(
  link_with : collectors_lib,
  include_directories : root_inc,
  dependencies : [
    common_dep,
    platform_deps,
  ],
)