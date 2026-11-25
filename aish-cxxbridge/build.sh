#!/usr/bin/env bash
set -euo pipefail

# Build the Rust static library and generate the C++ header used by the QML
# frontend. Run from anywhere; paths are resolved relative to this script.

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
profile="${PROFILE:-release}"
cxxbridge="${CXXBRIDGE:-/tmp/cxxbridge-tools/bin/cxxbridge}"

if [[ ! -x "$cxxbridge" ]]; then
    if command -v cxxbridge >/dev/null 2>&1; then
        cxxbridge="$(command -v cxxbridge)"
    else
        echo "error: cxxbridge not found; install it with:" >&2
        echo "  cargo install cxxbridge-cmd --version 1.0.205 --locked" >&2
        exit 1
    fi
fi

cargo build --profile "$profile" --manifest-path "$here/Cargo.toml"

mkdir -p "$here/generated/rust"
header="$here/generated/aish-cxxbridge.h"
cxx_header="$here/generated/rust/cxx.h"
tmp_header="$here/generated/.aish-cxxbridge.h.$$"
tmp_cxx_header="$here/generated/rust/.cxx.h.$$"
trap 'rm -f "$tmp_header" "$tmp_cxx_header"' EXIT

# Only replace headers when their contents change. Otherwise every bridge
# build touches the generated header and forces all dependent C++ objects to
# rebuild, defeating incremental builds.
"$cxxbridge" "$here/src/lib.rs" --header -o "$tmp_header"
"$cxxbridge" --header > "$tmp_cxx_header"
if ! cmp -s "$tmp_header" "$header"; then
    mv "$tmp_header" "$header"
fi
if ! cmp -s "$tmp_cxx_header" "$cxx_header"; then
    mv "$tmp_cxx_header" "$cxx_header"
fi

case "$profile" in
    release) lib_dir="$here/target/release" ;;
    dev)     lib_dir="$here/target/debug" ;;
    *)       lib_dir="$here/target/$profile" ;;
esac

echo "static library: $lib_dir/libaish_cxxbridge.a"
echo "header:         $here/generated/aish-cxxbridge.h"
