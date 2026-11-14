#!/bin/sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VARIANT="${1:-abi}"
ID=com.neoscaler.bpr1s-keymapper
VERSION=0.1.0
APP_DIR=usr/palm/applications/$ID
STAGE="$ROOT/dist/stage"
CTRL="$ROOT/dist/ctrl"
OUT="$ROOT/dist/${ID}_${VERSION}_all.ipk"

rm -rf "$STAGE" "$CTRL"
mkdir -p "$STAGE/$APP_DIR/bin" "$STAGE/var/lib/webosbrew/init.d" "$CTRL"

cp "$ROOT/packaging/appinfo.json" "$STAGE/$APP_DIR/"
cp "$ROOT/packaging/icon80.png" "$STAGE/$APP_DIR/"
cp "$ROOT/packaging/index.html" "$STAGE/$APP_DIR/"
cp "$ROOT/packaging/status.json" "$STAGE/$APP_DIR/"
cp "$ROOT/dist/bpr1s-keymapper.$VARIANT" "$STAGE/$APP_DIR/bin/bpr1s-keymapper"
chmod 755 "$STAGE/$APP_DIR/bin/bpr1s-keymapper"
cp "$ROOT/packaging/init.d-hook" "$STAGE/var/lib/webosbrew/init.d/60-bpr1s-keymapper"
chmod 755 "$STAGE/var/lib/webosbrew/init.d/60-bpr1s-keymapper"

cp "$ROOT/packaging/control" "$CTRL/control"
cp "$ROOT/packaging/postinst" "$CTRL/postinst"
cp "$ROOT/packaging/prerm" "$CTRL/prerm"
chmod 755 "$CTRL/postinst" "$CTRL/prerm"

printf '2.0\n' > "$ROOT/dist/debian-binary"
(cd "$STAGE" && tar --owner=0 --group=0 --numeric-owner -czf "$ROOT/dist/data.tar.gz" usr var)
(cd "$CTRL" && tar --owner=0 --group=0 --numeric-owner -czf "$ROOT/dist/control.tar.gz" control postinst prerm)
rm -f "$OUT"
ar rc "$OUT" "$ROOT/dist/debian-binary" "$ROOT/dist/control.tar.gz" "$ROOT/dist/data.tar.gz"
echo "built $OUT"
sha256sum "$OUT"
sh "$ROOT/tools/gen-repo-files.sh"
