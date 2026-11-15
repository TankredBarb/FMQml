#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT_DIR/build"
BUILD_JOBS="${BUILD_JOBS:-12}"

log() {
    printf '[install] %s\n' "$*"
}

fail() {
    printf '[install] error: %s\n' "$*" >&2
    exit 1
}

run_build_as_user() {
    if [[ "${EUID}" -eq 0 ]]; then
        local build_user="${SUDO_USER:-}"
        if [[ -z "$build_user" || "$build_user" == "root" ]]; then
            build_user="$(stat -c '%U' "$ROOT_DIR")"
        fi
        [[ -n "$build_user" && "$build_user" != "root" ]] || fail "run as a normal user or with sudo from a normal user; refusing to build as root"

        log "building as $build_user: cmake --build build -j $BUILD_JOBS"
        sudo -H -u "$build_user" cmake --build "$BUILD_DIR" -j "$BUILD_JOBS"
    else
        log "building: cmake --build build -j $BUILD_JOBS"
        cmake --build "$BUILD_DIR" -j "$BUILD_JOBS"
    fi
}

run_privileged_make() {
    local target="$1"

    if [[ "${EUID}" -eq 0 ]]; then
        log "running: make -C build $target"
        make -C "$BUILD_DIR" "$target"
    else
        command -v sudo >/dev/null 2>&1 || fail "sudo is required for make $target"
        log "running: sudo make -C build $target"
        sudo make -C "$BUILD_DIR" "$target"
    fi
}

[[ -d "$BUILD_DIR" ]] || fail "build directory does not exist; configure first with cmake -S . -B build ..."

if ! run_build_as_user; then
    fail "build failed; install/uninstall was not run"
fi

log "build succeeded"
if [[ -f "$BUILD_DIR/install_manifest.txt" ]]; then
    run_privileged_make uninstall
else
    log "no install manifest found; skipping uninstall for a clean installation"
fi
run_privileged_make install
if command -v dbus-send >/dev/null 2>&1 && [[ -S /run/dbus/system_bus_socket ]]; then
    space_helper_executable="$(sed -n 's/^Exec=//p' "$BUILD_DIR/io.fmqml.fm.Space.service")"
    if [[ -x "$space_helper_executable" ]]; then
        log "stopping the previous read-only space helper after update"
        if [[ "${EUID}" -eq 0 ]]; then
            "$space_helper_executable" --stop-running
        else
            sudo "$space_helper_executable" --stop-running
        fi
    fi
    log "refreshing system D-Bus policy for the read-only space helper"
    if [[ "${EUID}" -eq 0 ]]; then
        dbus-send --system --type=method_call --print-reply --dest=org.freedesktop.DBus / org.freedesktop.DBus.ReloadConfig
    else
        sudo dbus-send --system --type=method_call --print-reply --dest=org.freedesktop.DBus / org.freedesktop.DBus.ReloadConfig
    fi
fi
log "install complete"
