#!/bin/sh
# Build and test TheWatcher with Meson + Ninja on BSD systems.
# This wrapper builds the release configuration.
# Dependencies are built as local static Meson subprojects.

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "${SCRIPT_DIR}"

BUILD_DIR="builddir-release"

log() { printf '[meson-build] %s\n' "$*"; }

if ! command -v meson >/dev/null 2>&1; then
    printf '[meson-build] meson not found on PATH. Install with: ./scripts/setup-build-env-bsd.sh\n' >&2
    exit 1
fi

if [ ! -d "${BUILD_DIR}" ]; then
    log "Configuring ${BUILD_DIR} ..."
    meson setup "${BUILD_DIR}" --buildtype=release --default-library=static --backend=ninja
else
    log "Reconfiguring ${BUILD_DIR} ..."
    meson setup "${BUILD_DIR}" --reconfigure --buildtype=release --default-library=static --backend=ninja
fi

log "Compiling ..."
meson compile -C "${BUILD_DIR}"

log "Testing ..."
# `meson test --print-errorlogs` only echoes the last 100 lines of each
# failing test's stdout/stderr, which on a long-running integration test
# truncates the Catch2 REQUIRE source line + expansion that we actually
# need to diagnose flakes. On failure, dump the full testlog.txt to the
# console so CI logs preserve the assertion detail.
test_status=0
meson test -C "${BUILD_DIR}" --print-errorlogs || test_status=$?
if [ "${test_status}" -ne 0 ]; then
    log "Tests failed (exit ${test_status}); dumping full testlog.txt:"
    if [ -f "${BUILD_DIR}/meson-logs/testlog.txt" ]; then
        printf -- '----- BEGIN testlog.txt -----\n'
        cat "${BUILD_DIR}/meson-logs/testlog.txt"
        printf -- '----- END testlog.txt -----\n'
    else
        log "testlog.txt not found at ${BUILD_DIR}/meson-logs/testlog.txt"
    fi
    exit "${test_status}"
fi

log "Done."
