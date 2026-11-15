#!/bin/sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VARIANT="${1:-abi}"
ID=com.neoscaler.bpr1s-keymapper
VERSION=$(sed -n 's/^Version: //p' "$ROOT/packaging/control")
APP_DIR=usr/palm/applications/$ID
STAGE="$ROOT/dist/stage"
CTRL="$ROOT/dist/ctrl"
OUT="$ROOT/dist/${ID}_${VERSION}_all.ipk"
BIN="$ROOT/dist/bpr1s-keymapper.$VARIANT"

[ -n "$VERSION" ] || { echo "Version missing in packaging/control" >&2; exit 1; }
[ -x "$BIN" ] || { echo "$BIN missing - run tools/build.sh first" >&2; exit 1; }

rm -rf "$STAGE" "$CTRL"
mkdir -p "$STAGE/$APP_DIR/bin" "$STAGE/usr/palm/packages/$ID" "$CTRL"

sed "s/\"version\": *\"[^\"]*\"/\"version\": \"$VERSION\"/" \
    "$ROOT/packaging/appinfo.json" > "$STAGE/$APP_DIR/appinfo.json"
cp "$ROOT/packaging/icon80.png" "$STAGE/$APP_DIR/"
cp "$ROOT/packaging/index.html" "$STAGE/$APP_DIR/"
cp "$ROOT/packaging/status.json" "$STAGE/$APP_DIR/"
cp "$BIN" "$STAGE/$APP_DIR/bin/bpr1s-keymapper"
chmod 755 "$STAGE/$APP_DIR/bin/bpr1s-keymapper"
cp "$ROOT/packaging/init.d-hook" "$STAGE/$APP_DIR/bin/60-bpr1s-keymapper"
chmod 755 "$STAGE/$APP_DIR/bin/60-bpr1s-keymapper"
cat > "$STAGE/usr/palm/packages/$ID/packageinfo.json" <<EOF
{
  "app": "$ID",
  "id": "$ID",
  "loc_name": "BPR1S Plus Key Mapper",
  "package_format_version": 2,
  "vendor": "neoscaler",
  "version": "$VERSION"
}
EOF

cp "$ROOT/packaging/control" "$CTRL/control"
cp "$ROOT/packaging/postinst" "$CTRL/postinst"
cp "$ROOT/packaging/prerm" "$CTRL/prerm"
chmod 755 "$CTRL/postinst" "$CTRL/prerm"
SIZE_KB=$(du -sk "$STAGE" | cut -f1)
sed -i "s/^Architecture: all$/Architecture: all\\nInstalled-Size: $SIZE_KB/" "$CTRL/control"

printf '2.0\n' > "$ROOT/dist/debian-binary"
# webOS 5 pkgverifier rejects epoch-0 timestamps as "implausibly old".
IPK_MTIME=1700000000
(cd "$STAGE" && tar --sort=name --mtime="@$IPK_MTIME" --owner=0 --group=0 --numeric-owner -czf "$ROOT/dist/data.tar.gz" usr)
(cd "$CTRL" && tar --sort=name --mtime="@$IPK_MTIME" --owner=0 --group=0 --numeric-owner -czf "$ROOT/dist/control.tar.gz" control postinst prerm)
rm -f "$ROOT"/dist/${ID}_*.ipk
ar rcD "$OUT" "$ROOT/dist/debian-binary" "$ROOT/dist/control.tar.gz" "$ROOT/dist/data.tar.gz"
python3 - "$OUT" "$IPK_MTIME" <<'PY'
import sys
path, stamp = sys.argv[1], sys.argv[2].encode()
with open(path, "r+b") as f:
    data = bytearray(f.read())
    assert data[:8] == b"!<arch>\n", "not an ar archive"
    pos = 8
    while pos < len(data):
        header = data[pos:pos + 60]
        if header[58:60] != b"`\n":
            break
        data[pos + 16:pos + 28] = stamp.ljust(12)
        size = int(header[48:58].decode().strip())
        pos += 60 + size + (size % 2)
    f.seek(0)
    f.write(data)
PY
echo "built $OUT"
sha256sum "$OUT"
sh "$ROOT/tools/gen-repo-files.sh"
