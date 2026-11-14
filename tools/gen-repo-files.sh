#!/bin/sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ID=com.neoscaler.bpr1s-keymapper
VERSION=$(sed -n 's/^Version: //p' "$ROOT/packaging/control")
IPK="$ROOT/dist/${ID}_${VERSION}_all.ipk"
SHA=$(sha256sum "$IPK" | cut -d' ' -f1)
SIZE=$(stat -c%s "$IPK")
RAW="https://raw.githubusercontent.com/neoscaler/webos-bpr1s-keymapper"
IPK_URL="$RAW/v$VERSION/dist/${ID}_${VERSION}_all.ipk"
TAG="v$VERSION"

mkdir -p "$ROOT/api" "$ROOT/manifests"

cat > "$ROOT/manifests/${ID}.json" <<EOF
{
  "id": "$ID",
  "version": "$VERSION",
  "type": "web",
  "title": "BPR1S Key Mapper",
  "appDescription": "Maps the unused Home/OK/Back/Menu buttons of the Boxput BPR1S Plus remote on rooted LG webOS TVs.",
  "iconUri": "$RAW/main/packaging/icon80.png",
  "sourceUrl": "https://github.com/neoscaler/webos-bpr1s-keymapper",
  "rootRequired": true,
  "ipkUrl": "$IPK_URL",
  "ipkHash": { "sha256": "$SHA" },
  "ipkSize": $SIZE
}
EOF

cat > "$ROOT/api/apps.json" <<EOF
{
  "packages": [
    {
      "title": "BPR1S Key Mapper",
      "iconUri": "$RAW/main/packaging/icon80.png",
      "manifestUrl": "../manifests/${ID}.json",
      "category": "system",
      "pool": "main",
      "shortDescription": "Maps Home/OK/Back/Menu of the Boxput BPR1S Plus remote on LG webOS TVs (root)",
      "description": "Key mapper for the Boxput BPR1S Plus 2.4GHz remote on rooted LG webOS TVs. [Source](https://github.com/neoscaler/webos-bpr1s-keymapper)"
    }
  ]
}
EOF

case "$VERSION" in
*-*)
    BETA_MANIFEST="../manifests/${ID}-beta.json"
    BETA_PACKAGE="    {
      \"title\": \"BPR1S Key Mapper (beta $TAG)\",
      \"iconUri\": \"$RAW/main/packaging/icon80.png\",
      \"manifestUrl\": \"$BETA_MANIFEST\",
      \"category\": \"system\",
      \"pool\": \"beta\",
      \"shortDescription\": \"Pre-release $VERSION of the BPR1S key mapper (root)\",
      \"description\": \"Pre-release build $VERSION.\"
    }"
    cat > "$ROOT/manifests/${ID}-beta.json" <<EOF
{
  "id": "$ID",
  "version": "$VERSION",
  "type": "web",
  "title": "BPR1S Key Mapper",
  "appDescription": "Maps the unused Home/OK/Back/Menu buttons of the Boxput BPR1S Plus remote on rooted LG webOS TVs.",
  "iconUri": "$RAW/main/packaging/icon80.png",
  "sourceUrl": "https://github.com/neoscaler/webos-bpr1s-keymapper",
  "rootRequired": true,
  "ipkUrl": "$IPK_URL",
  "ipkHash": { "sha256": "$SHA" },
  "ipkSize": $SIZE
}
EOF
    ;;
*)
    rm -f "$ROOT/manifests/${ID}-beta.json"
    BETA_PACKAGE=
    ;;
esac

if [ -n "$BETA_PACKAGE" ]; then
    cat > "$ROOT/api/apps-beta.json" <<EOF
{
  "packages": [
$BETA_PACKAGE
  ]
}
EOF
else
    cat > "$ROOT/api/apps-beta.json" <<EOF
{
  "packages": []
}
EOF
fi

echo "repo files: api/apps.json, api/apps-beta.json, manifests/${ID}.json (sha256=$SHA)"
