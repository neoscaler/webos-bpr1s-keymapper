#!/bin/sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ZIG="${ZIG:-}"
if [ -z "$ZIG" ]; then
    if [ -x "$ROOT/tools/zig/zig" ]; then
        ZIG="$ROOT/tools/zig/zig"
    elif [ -x "$ROOT/../tools/zig/zig" ]; then
        ZIG="$ROOT/../tools/zig/zig"
    else
        echo "zig not found - run tools/get-zig.sh first or set ZIG=/path/to/zig" >&2
        exit 1
    fi
fi
mkdir -p "$ROOT/dist"

for target in arm-linux-musleabihf arm-linux-musleabi; do
    tag=$(echo "$target" | sed 's/^arm-linux-musl//; s/^e//')
    "$ZIG" cc -target "$target" -O2 -static -s -o "$ROOT/dist/bpr1s-keymapper.$tag" "$ROOT/src/bpr1s-keymapper.c"
    "$ZIG" cc -target "$target" -O2 -static -s -o "$ROOT/dist/pointer-test.$tag" "$ROOT/src/pointer-test.c"
done

cc -O2 -Wall -Wextra -o "$ROOT/dist/bpr1s-keymapper.native" "$ROOT/src/bpr1s-keymapper.c"

for f in "$ROOT"/dist/bpr1s-keymapper.* "$ROOT"/dist/pointer-test.*; do
    case "$f" in *.native) continue;; esac
    echo "== $f"
    readelf -h "$f" | grep -E 'Class|Machine|Type'
done
ls -la "$ROOT/dist"
