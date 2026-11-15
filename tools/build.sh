#!/bin/sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION=$(sed -n 's/^Version: //p' "$ROOT/packaging/control")
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
[ -n "$VERSION" ] || { echo "Version missing in packaging/control" >&2; exit 1; }
mkdir -p "$ROOT/dist"

for target in arm-linux-musleabihf arm-linux-musleabi; do
    tag=$(echo "$target" | sed 's/^arm-linux-musl//; s/^e//')
    "$ZIG" cc -target "$target" -O2 -static -s -DKEYMAPPER_VERSION="\"$VERSION\"" \
        -o "$ROOT/dist/bpr1s-keymapper.$tag" \
        "$ROOT/src/bpr1s-keymapper.c" "$ROOT/src/keymap.c"
    "$ZIG" cc -target "$target" -O2 -static -s -o "$ROOT/dist/inject-test.$tag" "$ROOT/src/inject-test.c"
done

cc -O2 -Wall -Wextra -DKEYMAPPER_VERSION="\"$VERSION\"" \
    -o "$ROOT/dist/bpr1s-keymapper.native" \
    "$ROOT/src/bpr1s-keymapper.c" "$ROOT/src/keymap.c"

cc -O2 -Wall -Wextra -Werror -std=c99 \
    -o "$ROOT/dist/keymap-test.native" "$ROOT/src/keymap.c" "$ROOT/src/keymap-test.c"
"$ROOT/dist/keymap-test.native"

for f in "$ROOT"/dist/bpr1s-keymapper.* "$ROOT"/dist/inject-test.*; do
    case "$f" in *.native) continue;; esac
    echo "== $f"
    readelf -h "$f" | grep -E 'Class|Machine|Type'
done
ls -la "$ROOT/dist"
