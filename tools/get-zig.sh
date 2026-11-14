#!/bin/sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER=0.14.1
DEST="$ROOT/tools/zig"
TARBALL="zig-x86_64-linux-$VER.tar.xz"
URL="https://ziglang.org/download/$VER/$TARBALL"
INDEX="https://ziglang.org/download/index.json"

if [ -x "$DEST/zig" ]; then
    echo "zig already present: $DEST/zig"
    exit 0
fi

fetch() {
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL -o "$2" "$1"
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$2" "$1"
    else
        python3 -c 'import sys, urllib.request; urllib.request.urlretrieve(sys.argv[1], sys.argv[2])' "$1" "$2"
    fi
}

cd "$ROOT/tools"
echo "downloading $URL"
fetch "$URL" "$TARBALL"
fetch "$INDEX" index.json
EXPECTED=$(python3 -c 'import json, sys; print(json.load(open("index.json"))[sys.argv[1]]["x86_64-linux"]["shasum"])' "$VER")
ACTUAL=$(sha256sum "$TARBALL" | cut -d' ' -f1)
rm -f index.json
if [ "$EXPECTED" != "$ACTUAL" ]; then
    echo "checksum mismatch: erwartet $EXPECTED, bekommen $ACTUAL" >&2
    rm -f "$TARBALL"
    exit 1
fi
if ! tar xJf "$TARBALL" 2>/dev/null; then
    python3 -c 'import sys, tarfile; tarfile.open(sys.argv[1]).extractall(".")' "$TARBALL"
fi
mv "zig-x86_64-linux-$VER" zig
rm -f "$TARBALL"
"$DEST/zig" version
