# Agent Collector Contract

Collectors are the agent-side modules that populate one shared
`SystemMetrics` snapshot before the agent sends it to the server.

## Interface

All collectors inherit from `thewatcher::agent::Collector` in
`agent/collectors/collector.hpp`:

```cpp
std::string_view name() const noexcept;
void update(SystemMetrics& metrics);
```

`name()` returns a stable diagnostic identifier. `update(SystemMetrics&)`
mutates the provided snapshot in place.

## Current Collectors

| Collector | Files | Metrics fields |
| --- | --- | --- |
| CPU | `cpu_collector.hpp/.cpp` | `metrics.cpu` |
| Memory | `memory_collector.hpp/.cpp` | `metrics.memory` |
| Disk | `disk_collector.hpp/.cpp` | `metrics.disks` |
| Temperature | `temperature_collector.hpp/.cpp` | `metrics.temperatures` |
| Process | `process_collector.hpp/.cpp` | `metrics.top_processes` |
| Network | `network_collector.hpp/.cpp` | `metrics.networks` |

The agent also fills static system fields in `Agent::fill_static_info()`:

```text
os_name
os_version
hostname
platform
uptime_seconds
```

## Collector Rules

- A collector owns any previous-sample state needed for rate calculations.
- A collector should replace its full vector field each update unless it is
  intentionally merging values.
- A collector must not send frames or talk to the server directly.
- A collector should avoid throwing for ordinary missing platform data. Prefer
  empty vectors or zero values when a metric is unavailable.
- Platform-specific implementations must keep Windows, Linux, and BSD builds
  compiling.
- Keep field names stable. Dashboard and API users consume these JSON names.

## Runtime Settings

The current runtime settings are:

- `collection_interval`: seconds between periodic metrics submissions.
- `process_limit`: number of top processes captured by `ProcessCollector`.
- `collector_config`: per-agent collector thresholds, enabled fixed disks,
  enabled network interfaces, process watches, and consecutive-reading counts.

The server persists these settings and returns them to the agent in
`CONFIG_UPDATE` after the agent sends `CONFIG_REQUEST`.

`collector_config` is defined in `common/collector_config.hpp`. CPU, memory,
and disk thresholds are absolute percentages. Network thresholds are combined
receive plus transmit megabits per second per interface. Process watches use
exact executable names and expected instance counts; `ProcessCollector` always
includes matching watched processes in `metrics.top_processes`, even when they
are outside the normal top-N sample.

## Adding A Collector

1. Add the metric struct and JSON mapping to `common/metrics.hpp`.
2. Add the field to `SystemMetrics`.
3. Add collector source/header files under `agent/collectors/`.
4. Add the files to `agent/collectors/BUILD.bazel`.
5. Register the collector in `Agent::Agent()` in `agent/agent.cpp`.
6. Add or update `agent_tests/collector_contract_test.cpp`.
7. Update `integration_tests/server_agent_integration_test.cpp` if the metric
   must be proven through the server API.
8. Update `dashboard/src/models.ts`, `dashboard/src/status.ts`, and
   `dashboard/src/main.tsx` if the metric is displayed.
9. Update docs and changelog.

More end-to-end extension guidance is in [Development](development.md).

## Verification

Run collector and integration tests:

```powershell
.\scripts\bazel.cmd test //agent_tests:collector_contract_test //integration_tests:server_agent_integration_test --verbose_failures --test_output=errors
```

If dashboard status mapping changes:

```powershell
cd dashboard
npm.cmd run build
npm.cmd test
```
