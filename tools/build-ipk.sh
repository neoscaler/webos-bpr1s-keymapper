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
SIZE_KB=$(du -sk "$STAGE" | cut -f1)
sed -i "s/^Architecture: all$/Architecture: all\\nInstalled-Size: $SIZE_KB/" "$CTRL/control"

rm -f "$ROOT"/dist/${ID}_*.ipk
# webOS 5 rejects GNU tar members and epoch-0 timestamps with -5.
python3 "$ROOT/tools/pack-ipk.py" "$STAGE" "$CTRL" "$OUT"
echo "built $OUT"
sh "$ROOT/tools/gen-repo-files.sh"
