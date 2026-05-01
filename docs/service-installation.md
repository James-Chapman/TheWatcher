# Service Installation

This is the focused Windows Service Control Manager command reference. Full
operator setup is in [Installation](installation.md), [Configuration](configuration.md),
and [Running](running.md).

The server and agent run in foreground mode by default. Use the service flags
only when installing or running under Windows Service Control Manager.

## Server Service

Install:

```powershell
.\bazel-bin\server\TheWatcherServer.exe --install-service --config C:\ProgramData\TheWatcher\server.json
```

Start and stop:

```powershell
sc.exe start TheWatcherServer
sc.exe stop TheWatcherServer
```

Uninstall:

```powershell
.\bazel-bin\server\TheWatcherServer.exe --uninstall-service
```

The installed command line runs the same executable with `--service`.

## Agent Service

Create or edit `C:\ProgramData\TheWatcher\TheWatcherAgent.conf`:

```text
THEWATCHER_SERVER=<server-host-or-ip>
SERVER_PUBLIC_KEY=<server-public-key>
```

Install:

```powershell
.\bazel-bin\agent\TheWatcherAgent.exe --install-service --config C:\ProgramData\TheWatcher\TheWatcherAgent.conf
```

You can also pass one-time overrides at install time:

```powershell
.\bazel-bin\agent\TheWatcherAgent.exe --install-service --config C:\ProgramData\TheWatcher\TheWatcherAgent.conf --server tcp://monitor.local:5555 --enrollment tcp://monitor.local:5556 --server-key <server-public-key>
```

Start and stop:

```powershell
sc.exe start TheWatcherAgent
sc.exe stop TheWatcherAgent
```

Uninstall:

```powershell
.\bazel-bin\agent\TheWatcherAgent.exe --uninstall-service
```

## Multiple Instances

Use `--service-name <name>` consistently with install, service mode, and
uninstall:

```powershell
.\bazel-bin\agent\TheWatcherAgent.exe --install-service --service-name TheWatcherAgentLab --config C:\ProgramData\TheWatcher\lab-agent.conf
sc.exe start TheWatcherAgentLab
.\bazel-bin\agent\TheWatcherAgent.exe --uninstall-service --service-name TheWatcherAgentLab
```

## Logs

Runtime logs are written beside the selected config file:

```text
C:\ProgramData\TheWatcher\server.log
C:\ProgramData\TheWatcher\agent.log
```

## Non-Windows

The service flags return an explanatory error outside Windows. Linux and BSD
service management should be handled by the host init system, such as systemd,
rc.d, or another supervisor, using normal foreground mode and explicit
`--config` paths.
