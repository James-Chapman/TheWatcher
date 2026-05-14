# Threat Model

This document records the security model for TheWatcher as of 0.6.x. It is a
working document for audit, design review, and regression testing.

## Assets

- Server CURVE secret key and approved agent public keys.
- Agent CURVE secret keys and pinned server public-key fingerprints.
- Dashboard session cookies and user password hashes.
- Monitoring data, alert history, maintenance state, runbooks, and group
  assignments.
- Server database contents and local configuration files.
- Outbound webhook destinations for alerts and scheduled reports.

## Actors

- Global administrators and operators with fleet-wide privileges.
- Group administrators, operators, and viewers scoped to assigned groups.
- Approved agents with a server-approved CURVE public key.
- Pending agents on the plaintext enrollment endpoint.
- Unauthenticated network clients that can reach the API, enrollment socket, or
  data socket.
- A malicious browser page trying to drive an authenticated user's dashboard
  session.
- A compromised admin account attempting to use webhooks to reach internal
  services from the server host.

## Trust Boundaries

- Browser to REST API: cookie-authenticated HTTP. Production deployments should
  terminate TLS at a reverse proxy and bind the API to a trusted interface.
- Dashboard JavaScript to browser DOM: React escapes text by default; runbook
  markdown is stored as text and not rendered as HTML by the current dashboard.
- Agent enrollment to server: plaintext ZeroMQ REP used only to request
  approval and receive the server public key. First approval is trust-on-first-
  use; later approvals are checked against the pinned fingerprint.
- Agent data plane to server: ZeroMQ ROUTER with CURVE when the server has a
  public key configured. ZAP admits only approved agent public keys.
- Server to database: local SQLite or PostgreSQL via configured DSN.
- Server to outbound webhooks: untrusted URLs stored by global operators/admins.

## Threats And Mitigations

### Spoofing

- Agent data spoofing is mitigated by CURVE plus ZAP approved-key checks.
- Enrollment spoofing is limited by manual approval and server-key pinning, but
  first approval still relies on administrator judgment.
- Dashboard user spoofing is mitigated by libsodium password hashes and random
  256-bit session tokens.

### Tampering

- API mutations require authenticated roles and group checks.
- Group-scoped users cannot mutate global users, groups, settings, pending
  enrollment, or legacy global runbook links.
- SQL writes use parameter binding in SQLite and PostgreSQL stores.
- Config files are KEY=VALUE only and are bounded by size during parsing.

### Repudiation

- Alerts, silences, maintenance windows, runbooks, and user operations store the
  acting username where the current API contract supports it.
- Logs record security-relevant actions such as login failures, agent approval,
  command queueing, and webhook rejection.

### Information Disclosure

- Session cookies are HttpOnly, SameSite=Strict, Secure, and scoped to `/`.
- Group-scoped API queries filter agents, alerts, users, metrics, and views by
  shared group membership.
- PostgreSQL DSNs are logged with password values redacted.
- The default generated server and agent configs restrict secret-key files to
  owner read/write on non-Windows platforms.

### Denial Of Service

- Login failures are rate-limited per username.
- Config parser file and line sizes are bounded to avoid unbounded local input.
- The API still needs a global request-body size limit in httplib or a fronting
  reverse proxy; this is a residual risk.
- Metrics and alert retention settings should be configured to limit database
  growth in production.

### Elevation Of Privilege

- Role checks distinguish global and group-scoped privileges.
- Browser-origin checks reject credentialed cross-origin unsafe methods when an
  `Origin` or absolute `Referer` header is present.
- Legacy global runbook mutations require `global_admin` because those runbooks
  are consumed by all alert generation.

## Webhook SSRF Controls

Alert and report webhook URLs are parsed by a shared validator. It accepts only
plain HTTP URLs supported by the current httplib build and rejects malformed
authorities, userinfo, invalid ports, localhost, loopback, private, link-local,
multicast, documentation, benchmark, and other non-public literal IP ranges.

Hostname targets that resolve to private IPs after DNS lookup remain a residual
risk. Deployments should restrict outbound egress at the host or network layer
when webhooks are enabled.

## Residual Risks

- The API server is plain HTTP. Use a reverse proxy for TLS, HSTS, request body
  limits, and external exposure.
- Initial enrollment is trust-on-first-use. Operators must verify pending agent
  identity before approval.
- The built-in first-run `thewatcher` account has a documented default password.
  It must be changed or disabled immediately after bootstrap.
- Hostname webhook SSRF cannot be completely prevented without DNS resolution
  and connect-time IP validation.
- Agent compromise can still produce false local metrics for that host. The
  server authenticates the agent identity, not the truth of host telemetry.
