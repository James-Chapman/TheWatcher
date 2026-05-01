# Architecture

TheWatcher has four primary runtime pieces: server, agent, common protocol, and
dashboard.

## Server

Files:

```text
server/main.cpp
server/server.cpp
server/api.cpp
server/store_sqlite.cpp
server/zap_handler.cpp
server/config.cpp
```

Responsibilities:

- Load or create `server.json`.
- Bind the ZeroMQ ROUTER data socket on `bind_address`.
- Bind the ZeroMQ REP enrollment socket on `enrollment_address`.
- Start the HTTP REST API on `api_host:api_port`.
- Persist agents and metrics through `Store`, currently `SqliteStore`.
- Approve/reject/delete enrolled agents.
- Dispatch queued commands to agents over the existing agent connection.
- Respond to agent config refresh requests.
- Mark approved connected agents offline after `offline_after_seconds`.

The server does not initiate network connections to agents.

## Agent

Files:

```text
agent/main.cpp
agent/config.cpp
agent/enrollment.cpp
agent/agent.cpp
agent/collectors/*
```

Responsibilities:

- Load or create `TheWatcherAgent.conf`.
- Generate a stable agent id and CURVE keypair on first run.
- Submit enrollment requests until approved or rejected.
- Establish a ZeroMQ DEALER connection to the server data endpoint.
- Collect metrics periodically and submit `METRICS` frames.
- Send `HEARTBEAT` frames.
- Send `CONFIG_REQUEST` after each metrics submission.
- Apply `CONFIG_UPDATE` responses.
- Execute server commands such as pause, resume, restart collectors, status, and
  disconnect.

The agent is always the side that establishes connections.

## Common Protocol

Files:

```text
common/protocol.hpp
common/commands.hpp
common/metrics.hpp
common/crypto.hpp
common/logging.hpp
common/windows_service.hpp
```

Protocol frames are msgpack-encoded `thewatcher::proto::Frame` values. Each
frame has a `FrameType`, `agent_id`, `timestamp_ms`, and typed payload.

Frame types:

| Frame | Direction | Purpose |
| --- | --- | --- |
| `HEARTBEAT` | Agent to server | Updates `last_seen`. |
| `METRICS` | Agent to server | Sends a structured `SystemMetrics` snapshot. |
| `COMMAND` | Server to agent | Queued operator command. |
| `COMMAND_ACK` | Agent to server | Command result. |
| `CONFIG_UPDATE` | Server to agent | Runtime settings response. |
| `ENROLL_REQUEST` | Agent to server | Enrollment request on the enrollment socket. |
| `ENROLL_RESPONSE` | Server to agent | Pending, approved, or rejected enrollment state. |
| `CONFIG_REQUEST` | Agent to server | Requests current runtime settings after metrics submission. |

Commands are defined in `common/commands.hpp`. Metrics are defined in
`common/metrics.hpp` and serialized to JSON for the REST API.

## Storage

Files:

```text
server/store.hpp
server/store_sqlite.cpp
server/store_sqlite.hpp
```

`Store` is the server storage boundary. `SqliteStore` persists:

- agent identity and enrollment state;
- approved/rejected flags;
- connected and maintenance state;
- collection interval and process limit;
- first seen and last seen timestamps;
- metric snapshots.

Schema migrations are implemented as additive `ALTER TABLE` calls guarded by
column checks in `SqliteStore::init_schema()`.

## REST API

Implemented in `server/api.cpp`.

Important endpoints:

| Endpoint | Purpose |
| --- | --- |
| `GET /api/agents` | List agents and runtime state. |
| `POST /api/agents/:id/approve` | Approve enrollment and add the agent key to ZAP. |
| `POST /api/agents/:id/reject` | Reject enrollment and remove the key from ZAP. |
| `DELETE /api/agents/:id` | Delete the agent and its metrics. |
| `GET /api/metrics` | Latest metrics row per agent. |
| `GET /api/metrics/:id?limit=N` | Metric history for one agent. |
| `POST /api/agents/:id/set_interval` | Persist and queue interval update. |
| `POST /api/agents/:id/set_process_limit` | Persist and queue process limit update. |
| `POST /api/agents/:id/pause` | Queue pause command. |
| `POST /api/agents/:id/resume` | Queue resume command. |
| `POST /api/agents/:id/restart_collectors` | Queue collector restart. |
| `POST /api/agents/:id/disconnect` | Queue graceful disconnect. |
| `POST /api/agents/:id/get_status` | Queue immediate metrics report. |

## Dashboard

Files:

```text
dashboard/src/api.ts
dashboard/src/status.ts
dashboard/src/main.tsx
dashboard/src/models.ts
dashboard/src/styles.css
```

The dashboard is a React + TypeScript + Vite app. It polls `/api/agents` and
`/api/metrics`, maps backend rows into dashboard view models, and provides
operator controls for enrollment, runtime settings, commands, maintenance, and
deletion.

## Data Flow

```text
Agent config -> enrollment REQ -> Server enrollment REP
Agent DEALER -> Server ROUTER
Agent METRICS/HEARTBEAT/CONFIG_REQUEST -> Server
Server CONFIG_UPDATE/COMMAND -> Agent
Agent COMMAND_ACK -> Server
Server REST API -> Dashboard
```

The dashboard talks only to the server API. It never talks directly to agents.
