#!/bin/sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ID=com.neoscaler.bpr1s-keymapper
VERSION=$(sed -n 's/^Version: //p' "$ROOT/packaging/control")
IPK="$ROOT/dist/${ID}_${VERSION}_all.ipk"
SHA=$(sha256sum "$IPK" | cut -d' ' -f1)
SIZE=$(stat -c%s "$IPK")
RAW="https://raw.githubusercontent.com/neoscaler/webos-bpr1s-keymapper/main"

mkdir -p "$ROOT/api" "$ROOT/manifests"

cat > "$ROOT/api/apps.json" <<EOF
{
  "packages": [
    {
      "title": "BPR1S Key Mapper",
      "iconUri": "$RAW/packaging/icon80.png",
      "manifestUrl": "../manifests/${ID}.json",
      "category": "system",
      "pool": "main",
      "shortDescription": "Mappt Home/OK/Zurueck/Menue der Boxput BPR1S Plus auf LG webOS TVs (root)",
      "description": "Key-Mapper fuer die Boxput BPR1S Plus 2.4GHz-Fernbedienung auf gerooteten LG webOS TVs. [Source](https://github.com/neoscaler/webos-bpr1s-keymapper)"
    }
  ]
}
EOF

cat > "$ROOT/manifests/${ID}.json" <<EOF
{
  "id": "$ID",
  "version": "$VERSION",
  "type": "web",
  "title": "BPR1S Key Mapper",
  "appDescription": "Maps the unused Home/OK/Back/Menu buttons of the Boxput BPR1S Plus remote on rooted LG webOS TVs.",
  "iconUri": "$RAW/packaging/icon80.png",
  "sourceUrl": "https://github.com/neoscaler/webos-bpr1s-keymapper",
  "rootRequired": true,
  "ipkUrl": "../dist/${ID}_${VERSION}_all.ipk",
  "ipkHash": { "sha256": "$SHA" },
  "ipkSize": $SIZE
}
EOF

echo "repo files: api/apps.json + manifests/${ID}.json (sha256=$SHA)"
