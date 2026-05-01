# Configuration

This document lists the server and agent config files, default paths, command
line overrides, and every supported option.

## Server Config

The server config is JSON.

Default paths:

```text
Windows: C:\ProgramData\TheWatcher\server.json
Linux/BSD: /etc/thewatcher/server.json
```

The server uses `--config <path>` when provided. Otherwise it uses the platform
default path. If the file does not exist, the server creates it and generates a
CURVE keypair.

Example:

```json
{
  "bind_address": "tcp://*:5555",
  "enrollment_address": "tcp://*:5556",
  "api_host": "0.0.0.0",
  "api_port": 8080,
  "db_path": "thewatcher.db",
  "db_type": "sqlite",
  "postgres_dsn": "",
  "offline_after_seconds": 60,
  "server_public_key": "<40-character-z85-public-key>",
  "server_secret_key": "<40-character-z85-secret-key>"
}
```

Options:

| Option | Default | Description |
| --- | --- | --- |
| `bind_address` | `tcp://*:5555` | ZeroMQ ROUTER endpoint for approved agents. Agents connect to this endpoint and submit metrics, heartbeats, command ACKs, and config requests. |
| `enrollment_address` | `tcp://*:5556` | ZeroMQ REP endpoint for plaintext enrollment requests. |
| `api_host` | `0.0.0.0` | HTTP REST API bind host. |
| `api_port` | `8080` | HTTP REST API port consumed by the dashboard. |
| `db_path` | `thewatcher.db` | SQLite database path when `db_type` is `sqlite`. Relative paths resolve from the server working directory. |
| `db_type` | `sqlite` | Storage backend. `sqlite` is implemented. `postgres` is reserved in config but not implemented by the current build. |
| `postgres_dsn` | empty | Reserved for future PostgreSQL support. |
| `offline_after_seconds` | `60` | Approved connected agents are marked disconnected when `last_seen` is older than this number of seconds. Set `0` or a negative value to disable automatic offline marking. |
| `server_public_key` | generated | Server CURVE public key. Give this to agents as `SERVER_PUBLIC_KEY` or `--server-key`. |
| `server_secret_key` | generated | Server CURVE secret key. Keep this private. |

If `server_public_key` is empty, the server does not enable CURVE on the data
socket. The default generated config enables CURVE.

## Agent Config

The agent config is `KEY=VALUE` text. Existing JSON agent configs remain
readable for compatibility, but new files are written as `TheWatcherAgent.conf`.

Default paths:

```text
Windows foreground user: %APPDATA%\TheWatcher\TheWatcherAgent.conf
Windows recommended service path: C:\ProgramData\TheWatcher\TheWatcherAgent.conf
Linux/BSD: ~/.config/thewatcher/TheWatcherAgent.conf
```

Use `--config <path>` for services and repeatable deployments. If the file does
not exist, the agent creates it and generates an agent id plus CURVE keypair.

Minimal example:

```text
THEWATCHER_SERVER=monitor.example.internal
SERVER_PUBLIC_KEY=<server-public-key>
```

Full example:

```text
THEWATCHER_SERVER=monitor.example.internal
SERVER_ADDRESS=tcp://monitor.example.internal:5555
ENROLLMENT_ADDRESS=tcp://monitor.example.internal:5556
SERVER_PUBLIC_KEY=<40-character-z85-server-public-key>
AGENT_ID=<generated-agent-id>
AGENT_PUBLIC_KEY=<40-character-z85-agent-public-key>
AGENT_SECRET_KEY=<40-character-z85-agent-secret-key>
COLLECTION_INTERVAL=30
PROCESS_LIMIT=25
```

Options:

| Option | Default | Description |
| --- | --- | --- |
| `THEWATCHER_SERVER` | `127.0.0.1` derived defaults | Hostname or IP of the server. The agent derives `tcp://<host>:5555` for data and `tcp://<host>:5556` for enrollment. |
| `SERVER_ADDRESS` | `tcp://127.0.0.1:5555` | Explicit data endpoint. Overrides the endpoint derived from `THEWATCHER_SERVER`. |
| `ENROLLMENT_ADDRESS` | `tcp://127.0.0.1:5556` | Explicit enrollment endpoint. Overrides the endpoint derived from `THEWATCHER_SERVER`. |
| `SERVER_PUBLIC_KEY` | empty | Server CURVE public key. Required when the server data socket has CURVE enabled. |
| `AGENT_ID` | generated UUID | Stable agent identity used for enrollment, routing, and dashboard rows. |
| `AGENT_PUBLIC_KEY` | generated | Agent CURVE public key sent during enrollment and approved by the server. |
| `AGENT_SECRET_KEY` | generated | Agent CURVE secret key. Keep this private. |
| `COLLECTION_INTERVAL` | `30` | Seconds between periodic metrics submissions. The server can update this through the dashboard. |
| `PROCESS_LIMIT` | `25` | Number of top processes collected by the process collector. The server can update this through the dashboard. |

## Agent CLI Overrides

Agent command-line options override loaded config values for that process:

```text
--config <path>
--server <tcp-endpoint>
--enrollment <tcp-endpoint>
--server-key <40-character-z85-key>
--service
--service-name <name>
--install-service
--uninstall-service
--genkey
--help
```

`--genkey` prints a new CURVE keypair and exits.

## Server CLI Options

```text
--config <path>
--service
--service-name <name>
--install-service
--uninstall-service
--genkey
--help
```

`--genkey` prints a new CURVE keypair and exits.

## Runtime Settings Flow

The dashboard writes per-agent runtime settings through the server API. The
server persists `collection_interval` and `process_limit` in the agents table.
After every metrics submission, the agent sends a `CONFIG_REQUEST`; the server
responds with `CONFIG_UPDATE` containing the current persisted settings.

Agents establish all ZeroMQ connections. The server never dials agents.
