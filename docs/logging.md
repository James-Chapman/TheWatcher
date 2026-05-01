# Logging

The server and agent use `common/SingleLog.hpp` directly. Do not add a wrapper
around `SingleLog`; it is intended to be included and used as-is.

## Log Locations

Foreground and service modes write logs beside the active config file:

```text
server config: <config-directory>\server.json
server log:    <config-directory>\server.log

agent config:  <config-directory>\TheWatcherAgent.conf
agent log:     <config-directory>\agent.log
```

When a config path has no parent directory, logs are written to the current
working directory.

Recommended Windows service locations:

```text
C:\ProgramData\TheWatcher\server.log
C:\ProgramData\TheWatcher\agent.log
```

## Levels

Each binary configures its own logging before runtime work begins:

- console logging at `INFO`;
- file logging at `TRACE`;
- immediate flushing for errors and critical messages.

`TheWatcherServer` and `TheWatcherAgent` call
`SingleLog::GetInstance().SetConsoleLogLevel(...)`,
`SetFileLogLevel(...)`, and `SetLogFilePath(...)` directly from their
`main.cpp` files.

## Macros

Runtime log writes should use the macros provided by `SingleLog.hpp`:

```cpp
LOG_INFO("Agent started");
LOGF_DEBUG("Queued command id=%s type=%u", command_id.c_str(), type);
LOG_WARNING("Enrollment stopped before approval");
```

Use `LOG_` macros for literal or already-built strings. Use `LOGF_` macros
when formatting values.

`LOG_FUNCTION_TRACE` creates a scope logger that emits entry and exit trace
messages. Use it on lifecycle, setup, config, and infrequent control-flow
functions. Avoid it in hot loops, per-frame handlers, and collector update
methods unless diagnosing a specific issue.

## Startup Diagnostics

Server foreground mode waits for the server worker thread to report startup
success before logging:

```text
Running. Press Ctrl+C to stop.
```

If ZeroMQ binding, CURVE setup, API startup, or another startup step fails, the
server logs a critical `Startup failed: ...` message and exits non-zero.

## Useful Commands

```powershell
Get-Content C:\ProgramData\TheWatcher\server.log -Tail 100
Get-Content C:\ProgramData\TheWatcher\agent.log -Tail 100
```

Search for enrollment:

```powershell
Select-String -Path C:\ProgramData\TheWatcher\server.log -Pattern "New agent|pending|approved|rejected"
```

Search for agent runtime errors:

```powershell
Select-String -Path C:\ProgramData\TheWatcher\agent.log -Pattern "Fatal|error|warning"
```

## Service Mode

Windows service mode uses the same log path rules as foreground mode. Always
install services with `--config <path>` so logs land in a stable writable
directory.

## CLI Output

The `--help`, `--genkey`, `--install-service`, and `--uninstall-service`
commands keep concise console output for scripting and operator feedback.
Runtime diagnostics go through `SingleLog`.
