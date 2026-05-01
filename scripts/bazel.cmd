@echo off
setlocal

set "GIT_BIN=C:\Program Files\Git\bin"
set "GIT_BASH=%GIT_BIN%\bash.exe"
set "GIT_USR_BIN=C:\Program Files\Git\usr\bin"

if not exist "%GIT_BASH%" (
    set "GIT_BIN=C:\Program Files (x86)\Git\bin"
    set "GIT_BASH=%GIT_BIN%\bash.exe"
    set "GIT_USR_BIN=C:\Program Files (x86)\Git\usr\bin"
)

if not exist "%GIT_BASH%" (
    echo Git Bash was not found. Run scripts\setup-build-env.cmd -SkipWsl first.
    exit /b 1
)

set "BAZEL_SH=%GIT_BASH%"
set "PATH=%GIT_BIN%;%GIT_USR_BIN%;%PATH%"

set "BAZEL_LAUNCHER="

for %%C in (bazelisk.exe bazelisk.cmd bazel.exe) do (
    if not defined BAZEL_LAUNCHER (
        for /f "delims=" %%P in ('where %%C 2^>NUL') do (
            if not defined BAZEL_LAUNCHER set "BAZEL_LAUNCHER=%%P"
        )
    )
)

if defined BAZEL_LAUNCHER (
    "%BAZEL_LAUNCHER%" %*
    exit /b %ERRORLEVEL%
)

echo Bazelisk or Bazel was not found on PATH. Run scripts\setup-build-env.cmd -SkipWsl first.
exit /b 1
