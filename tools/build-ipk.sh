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

[ -n "$VERSION" ] || { echo "Version fehlt in packaging/control" >&2; exit 1; }
[ -x "$BIN" ] || { echo "$BIN fehlt - erst tools/build.sh ausfuehren" >&2; exit 1; }

rm -rf "$STAGE" "$CTRL"
mkdir -p "$STAGE/$APP_DIR/bin" "$STAGE/var/lib/webosbrew/init.d" "$CTRL"

sed "s/\"version\": *\"[^\"]*\"/\"version\": \"$VERSION\"/" \
    "$ROOT/packaging/appinfo.json" > "$STAGE/$APP_DIR/appinfo.json"
cp "$ROOT/packaging/icon80.png" "$STAGE/$APP_DIR/"
cp "$ROOT/packaging/index.html" "$STAGE/$APP_DIR/"
cp "$ROOT/packaging/status.json" "$STAGE/$APP_DIR/"
cp "$BIN" "$STAGE/$APP_DIR/bin/bpr1s-keymapper"
chmod 755 "$STAGE/$APP_DIR/bin/bpr1s-keymapper"
cp "$ROOT/packaging/init.d-hook" "$STAGE/var/lib/webosbrew/init.d/60-bpr1s-keymapper"
chmod 755 "$STAGE/var/lib/webosbrew/init.d/60-bpr1s-keymapper"

cp "$ROOT/packaging/control" "$CTRL/control"
cp "$ROOT/packaging/postinst" "$CTRL/postinst"
cp "$ROOT/packaging/prerm" "$CTRL/prerm"
chmod 755 "$CTRL/postinst" "$CTRL/prerm"

printf '2.0\n' > "$ROOT/dist/debian-binary"
(cd "$STAGE" && tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -czf "$ROOT/dist/data.tar.gz" usr var)
(cd "$CTRL" && tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -czf "$ROOT/dist/control.tar.gz" control postinst prerm)
rm -f "$ROOT"/dist/${ID}_*.ipk
ar rcD "$OUT" "$ROOT/dist/debian-binary" "$ROOT/dist/control.tar.gz" "$ROOT/dist/data.tar.gz"
echo "built $OUT"
sha256sum "$OUT"
sh "$ROOT/tools/gen-repo-files.sh"
