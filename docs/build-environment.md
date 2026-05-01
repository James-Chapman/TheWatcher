# Build Environment

This document describes the supported local build setup for TheWatcher.
Installation and runtime steps are covered in [Installation](installation.md)
and [Running](running.md).

## Requirements

### Windows build

- Windows 10 or Windows 11, x64.
- PowerShell 5.1 or newer.
- `winget` from Microsoft App Installer.
- Git for Windows, including Git Bash.
- Bazelisk, which downloads the Bazel version selected by the project.
- Visual Studio 2022 Build Tools with the C++ workload and Windows SDK.

The setup script installs the Windows tools with `winget` when they are missing.
Run it from a normal PowerShell session unless you also request WSL setup. Use
the `.cmd` wrapper because it starts PowerShell with a process-scoped execution
policy bypass; it does not change the machine or user execution policy.

```powershell
.\scripts\setup-build-env.cmd -SkipWsl
```

This installs or checks Git for Windows, Bazelisk, and Visual Studio Build
Tools. It also writes `.bazelrc.user` and persists `BAZEL_SH` for the current
Windows user. These settings keep Bazel Windows target and host actions on Git
Bash instead of `C:\Windows\System32\bash.exe`. The script also runs
`bazel shutdown` after writing `.bazelrc.user` so an old Bazel server does not
keep stale environment settings.

### Optional WSL/Linux build

WSL is optional for the Windows build. Use it when you want to verify the Linux
build path from a Windows machine.

Run the setup script from an elevated PowerShell session:

```powershell
.\scripts\setup-build-env.cmd -InstallWsl -LinuxDistro Ubuntu
```

If WSL asks for a reboot or first-user setup, complete that step and rerun the
same command. The script installs the Linux package toolchain and installs
Bazelisk as `/usr/local/bin/bazelisk` with a `/usr/local/bin/bazel` symlink.

## Build Steps

Open a new PowerShell window after installing tools so `PATH` changes are
visible.

```powershell
bazelisk shutdown
.\scripts\bazel.cmd build //server:TheWatcherServer //agent:TheWatcherAgent --verbose_failures
.\scripts\bazel.cmd test //... --verbose_failures
```

Run the full server-agent integration path directly with:

```powershell
.\scripts\bazel.cmd test //integration_tests:server_agent_integration_test --verbose_failures --test_output=errors
```

That test starts the server, enrolls and approves an agent, verifies collected
metrics reach the server, sends a disconnect command, and shuts both sides down.

If the error mentions `rules_foreign_cc` and `pkgconfig_tool_msvc_build_`, the
host action environment is stale or Bazel started without `BAZEL_SH`. Rerun the
setup script, then run the build through `.\scripts\bazel.cmd` from the
repository root.

After setup and a new terminal, direct Bazelisk is also valid:

```powershell
bazelisk test //... --verbose_failures
```

The checked-in `.bazelrc` and generated `.bazelrc.user` both prefer Git Bash for
Bazel actions. This prevents the common failure where Bazel invokes the WSL
launcher at `C:\Windows\System32\bash.exe` and errors because no WSL distro is
initialized.

Build the dashboard separately with npm:

```powershell
cd dashboard
npm.cmd install
npm.cmd run build
npm.cmd test
```

If `//...` reports errors under `bazel-help-root` or `bazel-user-root`, those
are local Bazel output directories, not source packages. They are ignored by
`.bazelignore`; delete them or rerun from the repository root after pulling the
updated ignore file.

## WSL Build Steps

After WSL setup, run the Linux build from inside the distro:

```bash
cd /mnt/c/dev/TheWatcher
bazelisk test //... --verbose_failures
```

For better filesystem performance, clone the repository under the WSL home
directory and run the same command there.

## Troubleshooting

### `Windows Subsystem for Linux has no installed distributions`

Bazel found the WSL `bash.exe` launcher instead of Git Bash. Run:

```powershell
.\scripts\setup-build-env.cmd -SkipWsl
```

Then open a new PowerShell window and rerun:

```powershell
.\scripts\bazel.cmd test //... --verbose_failures
```

### `bazelisk` or `bazel` is not recognized

Open a new PowerShell window after installation. If it still fails, rerun the
setup script and check the `winget` output.

### Visual C++ compiler not detected

Rerun the setup script. If Visual Studio Build Tools was just installed, reboot
or open a new terminal before building again.
