# Changelog

All notable changes to TheWatcher are recorded here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [0.2.17] - 2026-05-01

### Changed
- Converted project logging call sites to use `SingleLog.hpp` `LOG_` and
  `LOGF_` macros instead of direct logging function calls.
- Configured `SingleLog` directly from each binary entry point instead of using
  a project wrapper.

### Added
- Added detailed debug and trace logging around server startup, enrollment,
  API agent actions, command dispatch, ZAP decisions, SQLite store mutations,
  agent enrollment, config loading, and service control paths.

## [0.2.16] - 2026-05-01

### Added
- Added component prerequisite requirements to the installation documentation.
- Added nginx and Apache httpd examples for publishing the dashboard on Linux
  and BSD, including production static hosting and development-only Vite
  reverse proxy setups.

## [0.2.15] - 2026-05-01

### Added
- Added complete operator documentation for installation, configuration,
  running, service operation, logging, and dashboard use.
- Added architecture and development documentation covering every major
  component and the end-to-end workflow for adding collectors.

### Changed
- Reworked the README into a documentation index and quick-start guide.
- Expanded existing build, dashboard, logging, service, and collector docs to
  match the current server and agent behavior.

## [0.2.14] - 2026-05-01

### Added
- Added `TheWatcherAgent.conf` support with `THEWATCHER_SERVER=<host>` for
  deriving agent data and enrollment endpoints.
- Added agent-originated config refresh requests after metrics submissions.
- Added server-side offline detection using configurable
  `offline_after_seconds`.

### Changed
- Persisted per-agent collection interval and process limit settings so config
  refresh responses can return current server-side settings.

## [0.2.13] - 2026-05-01

### Added
- Added a persisted maintenance state for agents so pause and resume
  acknowledgements are reflected in the dashboard.
- Added a row-level Resume action for agents that are already in maintenance.

### Changed
- Hid approve and reject controls once enrollment has been decided, leaving
  deletion as the way to start enrollment again.

### Fixed
- Fixed the agent management table header and row layout so the heading bar no
  longer overlaps the row content.

## [0.2.12] - 2026-05-01

### Added
- Added an end-to-end server-agent integration test that exercises enrollment,
  API approval, CURVE connection, real collector metrics, API-visible server
  storage, disconnect, and shutdown.
- Added server-observed agent connection state to `/api/agents`.

### Fixed
- Sent the agent disconnect ACK immediately before stopping the agent IO loop so
  the server can record the disconnect.

## [0.2.11] - 2026-05-01

### Added
- Added dashboard agent management for approving, rejecting, configuring, and
  deleting enrolled agents.
- Added persistent rejected enrollment state so rejected agents receive an
  enrollment rejection instead of remaining pending.
- Added dashboard API helpers and tests for agent management actions.

### Fixed
- Corrected pending enrollment responses so unapproved agents keep polling
  instead of treating a pending response as approved.

### Changed
- Renamed Bazel executable targets to `//server:TheWatcherServer` and
  `//agent:TheWatcherAgent`.

## [0.2.10] - 2026-04-30

### Fixed
- Enabled ZeroMQ CURVE support in the Windows libzmq build by patching sodium
  detection for Bazel-provided libsodium.
- Rebuilt the documented `//server:server` target as `server.exe` so the
  command-line server path is no longer stale.
- Added foreground startup signaling so server worker-thread failures are
  reported before printing the running message.

## [0.2.9] - 2026-04-30

### Added
- Added a common `SingleLog` wrapper and wired server, agent, API, ZAP, and
  Windows service runtime diagnostics through it.
- Added logging documentation covering default log locations, levels, and
  service-mode behavior.

## [0.2.8] - 2026-04-30

### Added
- Added the React, TypeScript, and Vite dashboard implementation using the
  supplied dark infrastructure table sample as the visual direction.
- Added dashboard health mapping, summary counts, expandable detail rows, API
  polling, and Vitest coverage for status behavior.
- Added shared Windows service helpers and wired server/agent flags for
  `--service`, `--install-service`, `--uninstall-service`, and
  `--service-name`.
- Added dashboard and service installation documentation.

### Changed
- Foreground mode remains the default for server and agent binaries.

## [0.2.7] - 2026-04-30

### Fixed
- Added the workspace-local `bazel-help-root` verification output directory to
  `.bazelignore` so `bazel test //...` does not scan generated Bazel install
  files as source packages.
- Documented the local output-root troubleshooting case.

## [0.2.6] - 2026-04-30

### Added
- Added `scripts/bazel.cmd` as the supported Windows Bazel launcher so
  `BAZEL_SH` is set before the Bazel server chooses its shell executable.

### Changed
- The setup script now persists `BAZEL_SH` in the user environment.
- Updated build documentation to use `scripts/bazel.cmd` for Windows builds.

## [0.2.5] - 2026-04-30

### Added
- Added `scripts/setup-build-env.cmd` as an execution-policy-safe launcher for
  the PowerShell build environment setup script.

### Changed
- Updated setup documentation to use the `.cmd` wrapper by default.

## [0.2.4] - 2026-04-30

### Fixed
- Added Git Bash `BAZEL_SH` and `PATH` settings to Bazel host action
  environments so `rules_foreign_cc` exec-tool builds do not fall back to the
  WSL `bash.exe` launcher.
- Updated the build environment script to write the host action settings and
  restart the Bazel server after changing `.bazelrc.user`.

## [0.2.3] - 2026-04-30

### Added
- Added `scripts/setup-build-env.ps1` to install/check Windows build tools,
  configure Bazel's Git Bash environment, and optionally provision WSL Linux
  build dependencies with Bazelisk.
- Added build environment documentation covering requirements, setup, build
  steps, WSL verification, and troubleshooting.

### Fixed
- Ensured Bazel Windows actions prefer Git Bash before the WSL `bash.exe`
  launcher in `PATH`.
- Changed the Windows libsodium version-header genrule to use `cmd.exe` instead
  of Bash so it does not fail when WSL has no installed distro.

## [0.2.2] - 2026-04-30

### Changed
- Moved straightforward third-party dependencies to Bzlmod registry modules:
  Catch2, cpp-httplib, and SQLite.
- Updated `bazel_skylib` to the selected registry version used by transitive
  dependencies.
- Updated Catch2 tests to use the BCR module's `catch2_main` target and modular
  Catch2 v3 headers.

### Fixed
- Added a Bazel 9 compatibility patch for the BCR cpp-httplib module's
  `cc_library` load.

## [0.2.1] - 2026-04-30

### Fixed
- Added Bazel 9 / Windows build fixes for `rules_foreign_cc`, Git Bash shell
  selection, and libsodium's in-place autotools configuration.
- Added a native Windows libsodium Bazel target and corrected libzmq's Windows
  static-library output name.
- Configured msgpack-cxx to use bundled predef headers instead of requiring
  Boost for endian detection.
- Included the protocol helpers in `server/api.cpp` and fixed command argument
  packing to use the declared `proto::pack` helper.
- Fixed `get_os_version()` so the Windows path does not instantiate POSIX
  `utsname`, and added BDD coverage for a non-empty OS version string.
- Defined `ZMQ_STATIC` and Windows socket link libraries in the cppzmq Bazel
  wrapper so tests link against the static libzmq build.
- Replaced Unix-style Windows `-l...` link options with MSVC `*.lib` names and
  linked `shell32.lib` from `agent_lib` for the agent config path helper.
- Linked `shell32.lib` from `server_lib` for the server config path helper.
- Aligned the agent collector contract around `Collector::name()` and
  `Collector::update(SystemMetrics&)`.
- Removed stale `collect()` / `getRates()` collector declarations and
  placeholder collector namespaces that did not match `agent/agent.cpp`.
- Updated CPU, disk, and network collectors to populate `SystemMetrics`
  directly instead of returning ad-hoc metric maps.
- Hardened process collector state tracking by adding the missing internal
  `cpu_percent` field used for top-process sorting.
- Updated `agent/agent.cpp` to use `std::make_unique` for collector ownership
  and safe `dynamic_cast` checks before changing process collector limits.

### Added
- BDD-style collector contract test covering polymorphic `name()` and
  `update(SystemMetrics&)` behavior.
- Collector contract documentation in `docs/collector-contract.md`.
- Bazel build notes in `docs/bazel-build.md`.

## [0.2.0] - 2026-04-29

### Added
- `server/server.hpp` / `server/server.cpp` — main `Server` class orchestrating
  the ZMQ ROUTER socket (metrics/commands data plane), enrollment REP socket,
  ZAP auth handler, REST API, and SQLite storage backend.
- `server/main.cpp` — server entry point with signal handling (`SIGINT`/`SIGTERM`),
  config loading, `--genkey` helper, and background thread management.
- `server/BUILD.bazel` — Bazel build rules; splits server into a `cc_library`
  (`server_lib`) and a `cc_binary` (`server`) so tests can link the library.
- `common_tests/protocol_test.cpp` — BDD-style tests for Frame round-trip
  encoding, EnrollRequest/Response payloads, CommandMessage, SystemMetrics
  nested serialization, and FrameType enum stability.
- `server_tests/store_test.cpp` — BDD-style tests covering all SQLite store
  operations: agent CRUD, upsert idempotency, approval, deletion, metrics
  insertion, limit queries, and `latest_metrics` per-agent aggregation.
- `server_tests/zap_handler_test.cpp` — BDD-style tests for ZAP key-set
  management: add, remove, duplicate add, harmless remove-of-absent key.
- Googletest `1.14.0` added to `MODULE.bazel` for test infrastructure.

### Fixed
- `server/api.cpp` — all four `CommandMessage::command_id` assignments were
  incorrectly casting `uint32_t` to `std::string`; replaced with
  `std::to_string(...)`.

## [0.1.0] - 2026-04-28

### Added
- Initial implementation by prior LLM pass:
  - `common/` — `metrics.hpp`, `protocol.hpp`, `commands.hpp`, `crypto.hpp`
  - `agent/` — `Agent` class with three `std::jthread` loops (IO, collection,
    heartbeat), six platform-aware metric collectors (CPU, memory, disk,
    network, process, temperature), config persistence, and enrollment client.
  - `server/` (partial) — REST API (`api.hpp/cpp`), SQLite store
    (`store_sqlite.hpp/cpp`), ZAP handler (`zap_handler.hpp/cpp`), server
    config (`config.hpp/cpp`), abstract `Store` interface (`store.hpp`).
  - Bazel build system: `MODULE.bazel`, `.bazelrc`, third-party BUILD stubs
    for libzmq, libsodium, cppzmq, msgpack-cxx, cpp-httplib, sqlite3.
