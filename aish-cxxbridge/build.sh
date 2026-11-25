#!/usr/bin/env bash
set -euo pipefail

# Build the Rust static library and generate the C++ header used by the QML
# frontend. Run from anywhere; paths are resolved relative to this script.

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
profile="${PROFILE:-release}"
cxxbridge="${CXXBRIDGE:-cxxbridge}"

if ! command -v "$cxxbridge" >/dev/null 2>&1; then
    echo "error: cxxbridge not found; install it with:" >&2
    echo "  cargo install cxxbridge-cmd --version 1.0.205 --locked" >&2
    exit 1
fi

cargo build --profile "$profile" --manifest-path "$here/Cargo.toml"

mkdir -p "$here/generated/rust"
"$cxxbridge" "$here/src/lib.rs" --header -o "$here/generated/aish-cxxbridge.h"
"$cxxbridge" --header > "$here/generated/rust/cxx.h"

if [[ "$profile" == "release" ]]; then
    lib_dir="$here/target/release"
else
    lib_dir="$here/target/$profile"
fi

echo "static library: $lib_dir/libaish_cxxbridge.a"
echo "header:         $here/generated/aish-cxxbridge.h"
