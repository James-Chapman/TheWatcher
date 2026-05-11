#!/usr/bin/env bash
# Set up the TheWatcher build environment on Linux.
#
# Installs the Meson + Ninja toolchain, the C++20 compiler stack, and Node.js
# for the dashboard. Detects the distribution from /etc/os-release and uses
# the matching package manager.
#
# Supported distributions:
#   - Debian / Ubuntu (apt-get)
#   - Fedora / RHEL / CentOS / Rocky / Alma (dnf, falling back to yum)
#   - Arch / Manjaro (pacman)
#   - openSUSE / SLES (zypper)
#   - Alpine (apk)
#
# Usage:
#   ./scripts/setup-build-env-linux.sh [options]
#
# Options:
#   --skip-packages     Do not install distro packages.
#   --skip-meson        Do not install Meson/Ninja via pip.
#   --skip-node         Do not install Node.js/npm (dashboard build).
#   --no-verify         Skip the post-install verification step.
#   -h, --help          Show this help and exit.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

SKIP_PACKAGES=0
SKIP_MESON=0
SKIP_NODE=0
NO_VERIFY=0

usage() {
    sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'
}

for arg in "$@"; do
    case "$arg" in
        --skip-packages)   SKIP_PACKAGES=1 ;;
        --skip-meson)      SKIP_MESON=1 ;;
        --skip-node)       SKIP_NODE=1 ;;
        --no-verify)       NO_VERIFY=1 ;;
        -h|--help)         usage; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; usage >&2; exit 2 ;;
    esac
done

log_step() { printf '\n==> %s\n' "$*"; }
log_info() { printf '    %s\n' "$*"; }
log_warn() { printf '    WARNING: %s\n' "$*" >&2; }
die()      { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

has_cmd() { command -v "$1" >/dev/null 2>&1; }

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    if has_cmd sudo; then
        SUDO="sudo"
    else
        die "This script needs root privileges or sudo to install packages."
    fi
fi

detect_distro() {
    if [ -r /etc/os-release ]; then
        # shellcheck disable=SC1091
        . /etc/os-release
        DISTRO_ID="${ID:-unknown}"
        DISTRO_LIKE="${ID_LIKE:-}"
    else
        DISTRO_ID="unknown"
        DISTRO_LIKE=""
    fi

    case "${DISTRO_ID}:${DISTRO_LIKE}" in
        debian:*|ubuntu:*|linuxmint:*|*:*debian*|*:*ubuntu*)
            PKG_FAMILY="debian" ;;
        fedora:*|rhel:*|centos:*|rocky:*|almalinux:*|*:*rhel*|*:*fedora*)
            PKG_FAMILY="rhel" ;;
        arch:*|manjaro:*|endeavouros:*|*:*arch*)
            PKG_FAMILY="arch" ;;
        opensuse*:*|sles:*|*:*suse*)
            PKG_FAMILY="suse" ;;
        alpine:*)
            PKG_FAMILY="alpine" ;;
        *)
            die "Unsupported distribution: ID=${DISTRO_ID} ID_LIKE=${DISTRO_LIKE}. Re-run with --skip-packages and install dependencies manually." ;;
    esac

    log_info "Detected distribution: ${DISTRO_ID} (family: ${PKG_FAMILY})"
}

install_packages_debian() {
    export DEBIAN_FRONTEND=noninteractive
    $SUDO apt-get update
    $SUDO apt-get install -y \
        build-essential clang lld \
        git pkg-config \
        autoconf automake libtool make cmake ninja-build \
        unzip zip curl ca-certificates \
        python3 python3-pip python3-venv \
        gawk
    if [ "$SKIP_NODE" -eq 0 ]; then
        $SUDO apt-get install -y nodejs npm
    fi
}

install_packages_rhel() {
    if has_cmd dnf; then
        PKG="$SUDO dnf install -y"
    elif has_cmd yum; then
        PKG="$SUDO yum install -y"
    else
        die "Neither dnf nor yum is available."
    fi
    $PKG gcc gcc-c++ clang lld \
        git pkgconfig \
        autoconf automake libtool make cmake ninja-build \
        unzip zip curl ca-certificates \
        python3 python3-pip \
        gawk
    if [ "$SKIP_NODE" -eq 0 ]; then
        $PKG nodejs npm
    fi
}

install_packages_arch() {
    $SUDO pacman -Sy --noconfirm --needed \
        base-devel clang lld \
        git pkgconf \
        autoconf automake libtool make cmake ninja \
        unzip zip curl ca-certificates \
        python python-pip \
        gawk
    if [ "$SKIP_NODE" -eq 0 ]; then
        $SUDO pacman -S --noconfirm --needed nodejs npm
    fi
}

install_packages_suse() {
    $SUDO zypper --non-interactive install -y \
        gcc gcc-c++ clang lld \
        git pkg-config \
        autoconf automake libtool make cmake ninja \
        unzip zip curl ca-certificates \
        python3 python3-pip \
        gawk
    if [ "$SKIP_NODE" -eq 0 ]; then
        $SUDO zypper --non-interactive install -y nodejs npm
    fi
}

install_packages_alpine() {
    $SUDO apk add --no-cache \
        build-base clang lld \
        git pkgconf \
        autoconf automake libtool make cmake samurai \
        unzip zip curl ca-certificates \
        bash python3 py3-pip \
        gawk linux-headers
    if [ "$SKIP_NODE" -eq 0 ]; then
        $SUDO apk add --no-cache nodejs npm
    fi
}

install_packages() {
    log_step "Installing system packages"
    case "$PKG_FAMILY" in
        debian) install_packages_debian ;;
        rhel)   install_packages_rhel ;;
        arch)   install_packages_arch ;;
        suse)   install_packages_suse ;;
        alpine) install_packages_alpine ;;
    esac
}

install_meson() {
    log_step "Installing Meson and Ninja"

    if ! has_cmd python3; then
        die "python3 not found; rerun with --skip-meson or install Python first."
    fi

    # Prefer the distro package if it is recent enough (meson >= 1.4), otherwise
    # install per-user via pip --break-system-packages-free path: --user.
    if has_cmd meson; then
        local current
        current="$(meson --version 2>/dev/null || true)"
        log_info "Existing meson: ${current:-unknown}"
    fi

    if python3 -m pip --version >/dev/null 2>&1; then
        python3 -m pip install --user --upgrade "meson>=1.4" ninja
    elif has_cmd pipx; then
        pipx install meson || pipx upgrade meson
        pipx install ninja || pipx upgrade ninja
    else
        log_warn "pip is not available; skipping Meson/Ninja installation. Install python3-pip and rerun."
        return
    fi

    case ":${PATH}:" in
        *":${HOME}/.local/bin:"*) ;;
        *) log_warn "Add \"\$HOME/.local/bin\" to your PATH to use pip --user installs (meson, ninja)." ;;
    esac
}

verify_environment() {
    log_step "Verification"

    local failed=0

    for tool in git make cmake pkg-config curl python3; do
        if has_cmd "$tool"; then
            log_info "$tool: $(command -v "$tool")"
        else
            log_warn "$tool not found on PATH"
            failed=1
        fi
    done

    if has_cmd g++; then
        log_info "g++:    $(g++ --version | head -n1)"
    elif has_cmd clang++; then
        log_info "clang++: $(clang++ --version | head -n1)"
    else
        log_warn "No C++ compiler (g++ or clang++) found on PATH"
        failed=1
    fi

    if has_cmd ninja; then
        log_info "ninja:  $(ninja --version)"
    elif has_cmd samu; then
        log_info "samu:   $(samu --version 2>&1 | head -n1)"
    else
        log_warn "ninja (or samu) not found; Meson builds will fail."
    fi

    if has_cmd meson; then
        log_info "meson:  $(meson --version)"
    elif [ -x "${HOME}/.local/bin/meson" ]; then
        log_info "meson:  ${HOME}/.local/bin/meson ($(${HOME}/.local/bin/meson --version))"
    else
        log_warn "meson not found; Meson builds will not work until \$HOME/.local/bin is on PATH."
    fi

    if [ "$SKIP_NODE" -eq 0 ]; then
        if has_cmd node; then
            log_info "node:   $(node --version)"
        else
            log_warn "node not found on PATH; dashboard build will not work."
        fi
        if has_cmd npm; then
            log_info "npm:    $(npm --version)"
        else
            log_warn "npm not found on PATH."
        fi
    fi

    if [ "$failed" -ne 0 ]; then
        die "One or more required tools are missing. See warnings above."
    fi
}

log_step "TheWatcher Linux build environment setup"
log_info "Repository: ${REPO_ROOT}"

detect_distro

if [ "$SKIP_PACKAGES" -eq 0 ]; then
    install_packages
else
    log_info "Skipping system package installation."
fi

if [ "$SKIP_MESON" -eq 0 ]; then
    install_meson
else
    log_info "Skipping Meson installation."
fi

if [ "$NO_VERIFY" -eq 0 ]; then
    verify_environment
fi

cat <<EOF

Setup complete.

Build with Meson:
    meson setup builddir-release --buildtype=release --default-library=static
    meson compile -C builddir-release
    meson test    -C builddir-release --print-errorlogs

Dashboard:
    cd dashboard && npm install && npm run build && npm test

If you installed Meson via pip --user, open a new shell or add
\$HOME/.local/bin to PATH before running the meson commands.
EOF
