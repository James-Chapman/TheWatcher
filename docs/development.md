# Development

This document covers the repository layout, common build/test commands, and the
work required to extend TheWatcher end to end.

## Repository Layout

```text
agent/              Agent runtime, config, enrollment, collectors.
agent/collectors/   CPU, memory, disk, temperature, process, network collectors.
agent_tests/        Agent config and collector contract tests.
common/             Shared protocol, commands, metrics, crypto, logging, service helpers.
common_tests/       Tests for shared helpers.
dashboard/          React + TypeScript + Vite dashboard.
dashboard_tests/    Dashboard test helpers or future browser tests.
docs/               Operator and developer documentation.
integration_tests/  End-to-end server-agent lifecycle tests.
patches/            Third-party source patches consumed by Bazel.
plans/              dev-task-planner task plans.
scripts/            Build environment and Bazel wrapper scripts.
server/             Server runtime, API, storage, ZAP auth, config.
server_tests/       Store, lifecycle, and ZAP tests.
third_party/        Bazel BUILD wrappers for archive-backed dependencies.
```

## Core Build Targets

```powershell
.\scripts\bazel.cmd build //server:TheWatcherServer //agent:TheWatcherAgent --verbose_failures
.\scripts\bazel.cmd test //agent_tests:config_test --verbose_failures
.\scripts\bazel.cmd test //server_tests:store_test --verbose_failures
.\scripts\bazel.cmd test //server_tests:server_lifecycle_test --verbose_failures
.\scripts\bazel.cmd test //server_tests:zap_handler_test --verbose_failures
.\scripts\bazel.cmd test //integration_tests:server_agent_integration_test --verbose_failures --test_output=errors
.\scripts\bazel.cmd test //... --verbose_failures
```

Dashboard:

```powershell
cd dashboard
npm.cmd install
npm.cmd run build
npm.cmd test
```

## Coding Standards

- C++ code is C++20.
- Format C++ with the repository `.clang-format`.
- Keep top-level C++ namespace as `thewatcher`.
- Prefer structured types in `common/metrics.hpp`, `common/protocol.hpp`, and
  `common/commands.hpp` over ad hoc strings.
- Use BDD-style tests with `GIVEN`, `WHEN`, and `THEN`.
- Add tests before behavior changes.
- Update docs for every behavior or workflow change.
- Bump `MODULE.bazel` version and update `CHANGELOG.md` for each change set.

## Adding A New Collector End To End

Use this checklist when adding a metric family such as GPU, filesystem inode
usage, services, packages, or custom application health.

1. Define the metric shape in `common/metrics.hpp`.

   Add a struct with stable field names and a
   `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` mapping. Add the struct to
   `SystemMetrics`.

2. Add wire/API compatibility tests.

   Update or add tests that serialize and inspect `SystemMetrics`. If the new
   metric affects integration behavior, extend
   `integration_tests/server_agent_integration_test.cpp`.

3. Implement the collector.

   Add `agent/collectors/<name>_collector.hpp` and `.cpp`. Inherit from
   `thewatcher::agent::Collector` and implement:

   ```cpp
   std::string_view name() const noexcept;
   void update(SystemMetrics& metrics);
   ```

   Keep platform-specific code behind preprocessor guards. The agent should
   still compile on Windows, Linux, and BSD.

4. Register the collector in Bazel.

   Update `agent/collectors/BUILD.bazel` with the new source and header.

5. Add the collector to the agent runtime.

   Register it in `Agent::Agent()` in `agent/agent.cpp`. If it needs runtime
   settings, add those settings to `common/commands.hpp`, `server/store.hpp`,
   `server/store_sqlite.cpp`, `server/api.cpp`, `dashboard/src/models.ts`, and
   the dashboard controls.

6. Update dashboard models and status mapping.

   Update `dashboard/src/models.ts` for the API shape, `dashboard/src/status.ts`
   for health classification, and `dashboard/src/main.tsx` if the UI needs new
   controls or columns.

7. Add tests.

   Add focused collector tests in `agent_tests/` or extend
   `collector_contract_test.cpp`. Add dashboard tests when health classification
   changes. Extend integration tests when the new metric must be proven through
   the server API.

8. Update docs.

   Update [Collector Contract](collector-contract.md), [Architecture](architecture.md),
   [Dashboard](dashboard.md), and `CHANGELOG.md`.

9. Run verification.

   ```powershell
   clang-format -i <changed-cpp-files>
   .\scripts\bazel.cmd test //agent_tests:collector_contract_test //integration_tests:server_agent_integration_test --verbose_failures --test_output=errors
   cd dashboard
   npm.cmd run build
   npm.cmd test
   ```

## Adding A New Server Command

1. Add the command enum and payload struct in `common/commands.hpp`.
2. Add an API route in `server/api.cpp` that validates input and queues a
   `CommandMessage`.
3. Handle the command in `Agent::handle_command()` in `agent/agent.cpp`.
4. If the command changes persistent state, update `server/store.hpp`,
   `server/store_sqlite.cpp`, the dashboard model, and API tests.
5. Add BDD tests and update docs.

## Adding A New REST API Field

1. Add the persistent or computed field in the server model.
2. Serialize it in `agent_to_json()` or metric JSON.
3. Update `dashboard/src/models.ts`.
4. Update `dashboard/src/status.ts` or `dashboard/src/main.tsx` if displayed.
5. Run dashboard build/tests and relevant server tests.

## Third-Party Dependencies

Prefer Bzlmod registry modules when they preserve behavior. Keep archive-backed
wrappers only where the project needs custom build settings or the dependency is
not available in the registry.

Current archive-backed dependencies:

- `libzmq`
- `libsodium`
- `cppzmq`
- `msgpack-cxx`

See [Bazel Build Notes](bazel-build.md) before changing third-party wrappers.

## Release Checklist

1. Add or update BDD tests.
2. Implement the change.
3. Run focused tests.
4. Run relevant integration tests.
5. Format C++ code.
6. Update docs.
7. Bump `MODULE.bazel`.
8. Add a `CHANGELOG.md` entry.
9. Run final build/test commands for changed components.
