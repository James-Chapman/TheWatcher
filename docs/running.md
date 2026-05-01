# Running TheWatcher

This document describes how to start, stop, and verify the server, agent, and
dashboard.

## Start The Server In Foreground Mode

```powershell
.\bazel-bin\server\TheWatcherServer.exe --config C:\ProgramData\TheWatcher\server.json
```

Expected log lines include:

```text
Config: C:\ProgramData\TheWatcher\server.json
Log: C:\ProgramData\TheWatcher\server.log
Public key: <server-public-key>
Running. Press Ctrl+C to stop.
```

Stop foreground mode with `Ctrl+C`.

## Start The Agent In Foreground Mode

Create or edit the agent config first:

```text
THEWATCHER_SERVER=127.0.0.1
SERVER_PUBLIC_KEY=<server-public-key>
```

Start:

```powershell
.\bazel-bin\agent\TheWatcherAgent.exe --config C:\ProgramData\TheWatcher\TheWatcherAgent.conf
```

The agent enrolls first. Until an operator approves it, enrollment remains
pending and the agent keeps retrying.

Stop foreground mode with `Ctrl+C`.

## Start And Stop Windows Services

```powershell
sc.exe start TheWatcherServer
sc.exe stop TheWatcherServer
sc.exe start TheWatcherAgent
sc.exe stop TheWatcherAgent
```

Install and uninstall commands are in [Installation](installation.md) and
[Service Installation](service-installation.md).

## Run The Dashboard

Development dashboard:

```powershell
cd dashboard
npm.cmd install
npm.cmd run dev
```

Open:

```text
http://127.0.0.1:5173
```

Vite proxies `/api` to `http://127.0.0.1:8080`.

Production dashboard bundle:

```powershell
cd dashboard
npm.cmd run build
```

The production files are written to `dashboard\dist`. The C++ server does not
serve these static files yet; use a separate static host if you need a deployed
dashboard.

## First Enrollment Workflow

1. Start the server.
2. Copy the server public key into the agent config as `SERVER_PUBLIC_KEY`.
3. Start the agent.
4. Open the dashboard Agents page.
5. Approve the pending agent.
6. Wait for the agent to reconnect with CURVE and submit metrics.

Rejected agents receive a rejection response on their next enrollment attempt.
Delete the rejected row to allow a fresh enrollment.

## Health Checks

Check server API:

```powershell
Invoke-RestMethod http://127.0.0.1:8080/api/agents
Invoke-RestMethod http://127.0.0.1:8080/api/metrics
```

Check logs:

```powershell
Get-Content C:\ProgramData\TheWatcher\server.log -Tail 50
Get-Content C:\ProgramData\TheWatcher\agent.log -Tail 50
```

Run the end-to-end integration test:

```powershell
.\scripts\bazel.cmd test //integration_tests:server_agent_integration_test --verbose_failures --test_output=errors
```

That test starts an isolated server, enrolls and approves an agent, verifies CPU,
memory, disk, temperature, process, network, OS, host, platform, and uptime data
reach the server, disconnects the agent, and shuts down.

## Offline Behavior

The server updates `last_seen` for every inbound agent frame. Approved connected
agents are marked disconnected when the server has not seen them for more than
`offline_after_seconds` from `server.json`.

Explicit disconnect still marks an agent disconnected immediately when the agent
ACKs the command.
