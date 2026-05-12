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
meson test -C "${BUILD_DIR}" --print-errorlogs

log "Done."
