@echo off
setlocal enableextensions

rem Build and test TheWatcher with Meson + Ninja.
rem This wrapper builds the release configuration.
rem Dependencies are built as local static Meson subprojects.

set "BUILD_DIR=builddir-release"

where meson >nul 2>&1
if errorlevel 1 (
    echo [meson-build] meson not found on PATH. Install with: pip install meson ninja
    exit /b 1
)

rem --backend=ninja stops Meson choosing the VS solution generator when MSVC
rem is present; harmless on hosts where ninja is the default backend anyway.
if not exist "%BUILD_DIR%" (
    echo [meson-build] Configuring %BUILD_DIR% ...
    meson setup "%BUILD_DIR%" --buildtype=release --default-library=static --backend=ninja || exit /b %ERRORLEVEL%
) else (
    echo [meson-build] Reconfiguring %BUILD_DIR% ...
    meson setup "%BUILD_DIR%" --reconfigure --buildtype=release --default-library=static --backend=ninja || exit /b %ERRORLEVEL%
)

echo [meson-build] Compiling ...
meson compile -C "%BUILD_DIR%" || exit /b %ERRORLEVEL%

echo [meson-build] Testing ...
meson test -C "%BUILD_DIR%" --print-errorlogs || exit /b %ERRORLEVEL%

echo [meson-build] Done.
exit /b 0
