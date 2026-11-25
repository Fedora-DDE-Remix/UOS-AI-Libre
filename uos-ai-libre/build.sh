#!/usr/bin/env bash
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
bridge_root="${AISH_CXXBRIDGE_ROOT:-$here/../aish-cxxbridge}"
cxxbridge="${CXXBRIDGE:-/tmp/cxxbridge-tools/bin/cxxbridge}"

if [[ ! -x "$cxxbridge" ]]; then
    if command -v cxxbridge >/dev/null 2>&1; then
        cxxbridge="$(command -v cxxbridge)"
    else
        echo "cxxbridge not found; set CXXBRIDGE=/path/to/cxxbridge" >&2
        exit 1
    fi
fi

CXXBRIDGE="$cxxbridge" PROFILE=release "$bridge_root/build.sh"

cmake -S "$here" -B "$here/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DAISH_CXXBRIDGE_ROOT="$bridge_root"
cmake --build "$here/build" -j"$(nproc)"
