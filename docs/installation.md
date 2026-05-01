# Installation

This document covers installing TheWatcher after the build environment is
ready. Build tool setup is covered in [Build Environment](build-environment.md).

## Prerequisites

### Server

- Windows 10/11 x64, Linux, or BSD.
- A writable config/log directory.
- Network listeners available for:
  - `5555/tcp` agent data and commands;
  - `5556/tcp` enrollment;
  - `8080/tcp` REST API.
- SQLite support from the built binary. No external SQLite service is required.
- The generated server CURVE public key copied to each agent config when CURVE
  is enabled.

### Agent

- Windows 10/11 x64, Linux, or BSD.
- Outbound network access to the server on `5555/tcp` and `5556/tcp`.
- A writable config/log directory.
- `THEWATCHER_SERVER=<server-host-or-ip>` in `TheWatcherAgent.conf`.
- `SERVER_PUBLIC_KEY=<server-public-key>` when the server data socket uses
  CURVE encryption.
- User-space permission to read the local OS counters used by the collectors.

### Dashboard

- Node.js and npm for building or running the Vite dev server.
- Server REST API reachable from the browser or from the web server proxy.
- For development: Vite dev server on `127.0.0.1:5173`.
- For public use: nginx, Apache httpd, or another static web server serving
  `dashboard/dist` and proxying `/api` to the TheWatcher server API.

The Vite dev server is intended for development. For public access, build the
dashboard and serve the static files with a web server.

### Build Machine

- Bazelisk and the C++ toolchain described in [Build Environment](build-environment.md).
- Git Bash on Windows for Bazel third-party builds.
- Visual Studio 2022 Build Tools on Windows.
- npm for dashboard build and tests.

## Build Artifacts

Build the server and agent from the repository root:

```powershell
.\scripts\bazel.cmd build //server:TheWatcherServer //agent:TheWatcherAgent --verbose_failures
```

The Windows binaries are written under:

```text
bazel-bin\server\TheWatcherServer.exe
bazel-bin\agent\TheWatcherAgent.exe
```

The dashboard production bundle is written to `dashboard\dist`:

```powershell
cd dashboard
npm.cmd install
npm.cmd run build
```

The C++ server currently exposes the REST API. It does not serve the dashboard
production bundle itself, so run the Vite dev server during development or host
`dashboard\dist` with a separate static web server.

## Linux And BSD Dashboard Publishing

Build the dashboard on the host that will publish it:

```bash
cd dashboard
npm install
npm run build
```

Copy or keep the generated `dashboard/dist` directory somewhere readable by the
web server. The examples below use:

```text
/opt/thewatcher/dashboard/dist
```

The examples assume the TheWatcher server API listens on `127.0.0.1:8080`.

### nginx Static Dashboard

Recommended public deployment: nginx serves the static dashboard and proxies
only `/api` to the TheWatcher server.

```nginx
server {
    listen 80;
    server_name watcher.example.com;

    root /opt/thewatcher/dashboard/dist;
    index index.html;

    location /api/ {
        proxy_pass http://127.0.0.1:8080/api/;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

### Apache httpd Static Dashboard

Enable the required proxy modules if they are not already enabled. Module names
vary by distribution, but Apache needs `mod_proxy`, `mod_proxy_http`,
`mod_headers`, and normal static file serving.

```apache
<VirtualHost *:80>
    ServerName watcher.example.com
    DocumentRoot "/opt/thewatcher/dashboard/dist"

    <Directory "/opt/thewatcher/dashboard/dist">
        Require all granted
        Options -Indexes
        AllowOverride None
    </Directory>

    ProxyPreserveHost On
    ProxyPass        "/api/" "http://127.0.0.1:8080/api/"
    ProxyPassReverse "/api/" "http://127.0.0.1:8080/api/"

    RewriteEngine On
    RewriteCond "%{REQUEST_FILENAME}" !-f
    RewriteCond "%{REQUEST_FILENAME}" !-d
    RewriteRule "^/(.*)$" "/index.html" [L]
</VirtualHost>
```

### Development-Only nginx Proxy To Vite

Use this only when actively developing the dashboard. Keep Vite bound to
loopback and let nginx expose it.

```bash
cd dashboard
npm run dev -- --host 127.0.0.1
```

```nginx
server {
    listen 80;
    server_name watcher-dev.example.com;

    location /api/ {
        proxy_pass http://127.0.0.1:8080/api/;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    location / {
        proxy_pass http://127.0.0.1:5173/;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }
}
```

### Development-Only Apache httpd Proxy To Vite

Use this only when actively developing the dashboard.

```apache
<VirtualHost *:80>
    ServerName watcher-dev.example.com

    ProxyPreserveHost On
    ProxyPass        "/api/" "http://127.0.0.1:8080/api/"
    ProxyPassReverse "/api/" "http://127.0.0.1:8080/api/"

    ProxyPass        "/" "http://127.0.0.1:5173/"
    ProxyPassReverse "/" "http://127.0.0.1:5173/"
</VirtualHost>
```

## Recommended Windows Layout

Use `C:\ProgramData\TheWatcher` for service configs and logs:

```powershell
New-Item -ItemType Directory -Force C:\ProgramData\TheWatcher
```

Recommended files:

```text
C:\ProgramData\TheWatcher\server.json
C:\ProgramData\TheWatcher\server.log
C:\ProgramData\TheWatcher\TheWatcherAgent.conf
C:\ProgramData\TheWatcher\agent.log
```

The server creates `server.json` on first run if it does not exist. The agent
creates `TheWatcherAgent.conf` on first run if it does not exist.

## Foreground Install

Foreground mode is the simplest local setup. No service registration is needed.

Create the server config and print its public key:

```powershell
.\bazel-bin\server\TheWatcherServer.exe --config C:\ProgramData\TheWatcher\server.json
```

Stop it with `Ctrl+C`, copy the logged public key into the agent config, then
start the server again when ready.

Create or edit `C:\ProgramData\TheWatcher\TheWatcherAgent.conf`:

```text
THEWATCHER_SERVER=127.0.0.1
SERVER_PUBLIC_KEY=<server-public-key>
```

Start the agent:

```powershell
.\bazel-bin\agent\TheWatcherAgent.exe --config C:\ProgramData\TheWatcher\TheWatcherAgent.conf
```

Open the dashboard and approve the pending agent.

## Windows Services

Install the server service:

```powershell
.\bazel-bin\server\TheWatcherServer.exe --install-service --config C:\ProgramData\TheWatcher\server.json
```

Install the agent service:

```powershell
.\bazel-bin\agent\TheWatcherAgent.exe --install-service --config C:\ProgramData\TheWatcher\TheWatcherAgent.conf --server-key <server-public-key>
```

Start and stop services:

```powershell
sc.exe start TheWatcherServer
sc.exe stop TheWatcherServer
sc.exe start TheWatcherAgent
sc.exe stop TheWatcherAgent
```

Uninstall services:

```powershell
.\bazel-bin\server\TheWatcherServer.exe --uninstall-service
.\bazel-bin\agent\TheWatcherAgent.exe --uninstall-service
```

Use `--service-name <name>` on install, service mode, and uninstall when
running multiple instances on one machine.

## Linux And BSD

Linux and BSD builds use the same Bazel targets. Run the binaries in foreground
mode under a host supervisor such as systemd, rc.d, or another service manager.
The Windows service flags intentionally return an explanatory error outside
Windows.

Typical config paths:

```text
/etc/thewatcher/server.json
~/.config/thewatcher/TheWatcherAgent.conf
```

For daemon deployments, prefer explicit config paths owned by the service user
and pass `--config <path>` in the service definition.

## Network Ports

Defaults:

- `5555/tcp`: ZeroMQ agent data, heartbeats, command ACKs, and config refresh.
- `5556/tcp`: ZeroMQ enrollment.
- `8080/tcp`: HTTP REST API.
- `5173/tcp`: Vite dashboard dev server.

Agents connect outbound to the server. The server does not initiate network
connections to agents.
