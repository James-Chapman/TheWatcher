# Dashboard

The dashboard is a React + TypeScript + Vite app under `dashboard/`.

## Role

The dashboard is the operator UI. It talks only to the server REST API; it does
not connect to agents.

Current capabilities:

- overview of agent health and latest metrics;
- pending enrollment approval/rejection;
- approved/rejected/maintenance grouping;
- poll interval and process limit updates;
- pause/resume maintenance mode;
- restart collectors;
- request immediate status;
- delete stale or rejected agent records.

## Development

Install dependencies:

```powershell
cd dashboard
npm.cmd install
```

Run the dev server:

```powershell
npm.cmd run dev
```

Vite serves the dashboard at `http://127.0.0.1:5173` and proxies `/api` to
`http://127.0.0.1:8080`.

## Build

```powershell
cd dashboard
npm.cmd run build
```

The production bundle is written to `dashboard/dist/`. The C++ server does not
serve this bundle yet. Host it separately for production use.

## Tests

```powershell
cd dashboard
npm.cmd test
```

The current tests cover health thresholds, worst-status summary counts,
missing-metrics behavior, rejected enrollment grouping, maintenance state,
agent row action state, and agent management API calls.

## Views

### Overview

The Overview view shows health state by agent and expands each row into current
component metrics. The dashboard joins `/api/agents` and `/api/metrics` by
`agent_id`.

### Agents

The Agents view manages enrollment and operations:

- pending agents show Approve and Reject;
- approved/rejected agents no longer show enrollment decision buttons;
- rejected agents should be deleted before retrying enrollment;
- approved agents can update poll interval and process limit;
- approved agents can enter or leave maintenance mode;
- approved agents can restart collectors, request status, or disconnect;
- any agent row can be deleted.

## API Contract

The dashboard consumes:

| Endpoint | Purpose |
| --- | --- |
| `GET /api/agents` | Agent identity, enrollment, connection, maintenance, settings, and timestamps. |
| `GET /api/metrics` | Latest metrics snapshot per agent. |
| `POST /api/agents/:id/approve` | Approve pending enrollment. |
| `POST /api/agents/:id/reject` | Reject pending enrollment. |
| `DELETE /api/agents/:id` | Delete agent and metrics. |
| `POST /api/agents/:id/set_interval` | Persist and queue poll interval update. |
| `POST /api/agents/:id/set_process_limit` | Persist and queue process limit update. |
| `POST /api/agents/:id/pause` | Queue maintenance pause. |
| `POST /api/agents/:id/resume` | Queue maintenance resume. |
| `POST /api/agents/:id/restart_collectors` | Queue collector restart. |
| `POST /api/agents/:id/get_status` | Queue immediate metrics report. |

Agent rows include:

```text
agent_id
hostname
platform
curve_public_key_z85
approved
rejected
connected
maintenance
collection_interval
process_limit
first_seen
last_seen
```

Metric rows include:

```text
agent_id
timestamp_ms
metrics
```

`metrics` is the JSON form of `common/metrics.hpp::SystemMetrics`.

## State Rules

- Pending agents receive `approved=false` until an operator approves them.
- Rejected agents receive an enrollment rejection on the next attempt.
- `connected` becomes true after inbound agent traffic.
- `connected` becomes false after successful disconnect ACK or after
  `offline_after_seconds` with no inbound frame.
- `maintenance` becomes true after successful pause ACK.
- `maintenance` becomes false after successful resume ACK.
- Runtime settings are persisted on the server and returned to agents after
  each metrics submission via config refresh.

## Changing Dashboard Data

When server API fields change:

1. Update `dashboard/src/models.ts`.
2. Update `dashboard/src/status.ts` if health or grouping changes.
3. Update `dashboard/src/main.tsx` if rendering or controls change.
4. Add/update Vitest coverage.
5. Run `npm.cmd run build` and `npm.cmd test`.
