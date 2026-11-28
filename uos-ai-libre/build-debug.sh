#!/usr/bin/env bash
set -euo pipefail

# Incremental debug build for both the Rust bridge and the Qt/C++ frontend.
# Reuses build-debug/ and Cargo's target/debug cache; safe to run repeatedly.

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
bridge_root="${AISH_CXXBRIDGE_ROOT:-$here/../vendor/aish-cxxbridge}"
build_dir="$here/build-debug"
jobs="${JOBS:-$(nproc)}"

echo "==> Rust bridge (dev profile, incremental)"
PROFILE=dev "$bridge_root/build.sh"

rust_lib="$bridge_root/target/debug/libaish_cxxbridge.a"
if [[ ! -f "$rust_lib" ]]; then
    echo "error: missing debug static library: $rust_lib" >&2
    exit 1
fi

echo "==> Qt/C++ frontend (Debug, incremental)"
cmake -S "$here" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DAISH_CXXBRIDGE_ROOT="$bridge_root" \
    -DAISH_CXXBRIDGE_LIB="$rust_lib" \
    -DAISH_CXXBRIDGE_INCLUDE="$bridge_root/generated"
cmake --build "$build_dir" -j"$jobs"

# qt_add_qml_module must have produced the build-time QML cache loader. If it
# is missing, the app would fall back to parsing QML at startup.
qml_cache_loader="$build_dir/.rcc/qmlcache/uos-ai-libre_qmlcache_loader.cpp"
if [[ ! -f "$qml_cache_loader" ]]; then
    echo "error: build-time QML cache was not generated: $qml_cache_loader" >&2
    exit 1
fi

echo
echo "debug binary: $build_dir/uos-ai-libre"
echo "compile commands: $build_dir/compile_commands.json"
