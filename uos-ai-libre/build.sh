#!/usr/bin/env bash
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
bridge_root="${AISH_CXXBRIDGE_ROOT:-$here/../vendor/aish-cxxbridge}"

resolve_cxxbridge() {
    if [[ -n "${CXXBRIDGE:-}" ]]; then
        printf '%s\n' "$CXXBRIDGE"
    elif command -v cxxbridge >/dev/null 2>&1; then
        command -v cxxbridge
    elif [[ -x "$HOME/.cargo/bin/cxxbridge" ]]; then
        printf '%s\n' "$HOME/.cargo/bin/cxxbridge"
    else
        echo "cxxbridge not found; installing cxxbridge-cmd 1.0.205 via cargo..." >&2
        cargo install cxxbridge-cmd --version 1.0.205 --locked >&2
        if [[ -x "$HOME/.cargo/bin/cxxbridge" ]]; then
            printf '%s\n' "$HOME/.cargo/bin/cxxbridge"
        elif command -v cxxbridge >/dev/null 2>&1; then
            command -v cxxbridge
        else
            echo "cargo install finished but cxxbridge is still not found" >&2
            exit 1
        fi
    fi
}

cxxbridge="$(resolve_cxxbridge)"

CXXBRIDGE="$cxxbridge" PROFILE=release "$bridge_root/build.sh"

cmake -S "$here" -B "$here/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DAISH_CXXBRIDGE_ROOT="$bridge_root"
cmake --build "$here/build" -j"$(nproc)"

# qt_add_qml_module must have produced the build-time QML cache loader. If it
# is missing, the app would fall back to parsing QML at startup.
qml_cache_loader="$here/build/.rcc/qmlcache/uos-ai-libre_qmlcache_loader.cpp"
if [[ ! -f "$qml_cache_loader" ]]; then
    echo "error: build-time QML cache was not generated: $qml_cache_loader" >&2
    exit 1
fi
