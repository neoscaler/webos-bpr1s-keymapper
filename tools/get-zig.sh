#!/bin/sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER=0.14.1
DEST="$ROOT/tools/zig"
if [ -x "$DEST/zig" ]; then
    echo "zig already present: $DEST/zig"
    exit 0
fi
URL="https://ziglang.org/download/$VER/zig-x86_64-linux-$VER.tar.xz"
cd "$ROOT/tools"
echo "downloading $URL"
if command -v curl >/dev/null 2>&1; then
    curl -fL -o zig.tar.xz "$URL"
elif command -v wget >/dev/null 2>&1; then
    wget -q -O zig.tar.xz "$URL"
else
    python3 -c "import urllib.request,sys; urllib.request.urlretrieve(sys.argv[1], sys.argv[2])" "$URL" zig.tar.xz
fi
if ! tar xJf zig.tar.xz 2>/dev/null; then
    python3 -c "import tarfile; tarfile.open('zig.tar.xz').extractall('.')"
fi
mv "zig-x86_64-linux-$VER" zig
rm -f zig.tar.xz
"$DEST/zig" version
