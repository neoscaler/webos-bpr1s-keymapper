#!/bin/sh
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ID=com.neoscaler.bpr1s-keymapper
VERSION=$(sed -n 's/^Version: //p' "$ROOT/packaging/control")
IPK="$ROOT/dist/${ID}_${VERSION}_all.ipk"
SHA=$(sha256sum "$IPK" | cut -d' ' -f1)
SIZE=$(stat -c%s "$IPK")
RAW="https://raw.githubusercontent.com/neoscaler/webos-bpr1s-keymapper"
PAGES="https://neoscaler.github.io/webos-bpr1s-keymapper"
ICON_URL="$PAGES/packaging/icon160.png"
IPK_URL="$RAW/v$VERSION/dist/${ID}_${VERSION}_all.ipk"
TAG="v$VERSION"
APP_DESCRIPTION="Maps the unused Home, OK, Back and Menu keys of the Boxput BPR1S Plus 2.4 GHz USB remote on rooted LG webOS 5 and 6 TVs. Runs as a root service at boot; all other keys pass through unchanged. A minimalist alternative to LG's bulky stock remote."
SHORT_DESCRIPTION="Root-only key mapper for the Boxput BPR1S Plus remote on LG webOS TVs."
LONG_DESCRIPTION="Root-only background service for LG webOS TVs (tested on webOS 5, 32-bit userland required). Maps the keys webOS ignores on the Boxput BPR1S Plus 2.4 GHz USB remote: Home and Menu open the launcher, OK sends ENTER, Back sends the real Back key. All other keys pass through unchanged. The daemon never grabs the dongle and filters out its pointer/gyro interface, so it stays idle at 0 percent CPU. Autostarts via the Homebrew Channel init.d hook and is fully removed on uninstall. [Source](https://github.com/neoscaler/webos-bpr1s-keymapper)"
INSTALLED_SIZE=$(python3 - "$IPK" <<'EOF'
import io, sys, tarfile

raw = open(sys.argv[1], "rb").read()
idx = raw.find(b"control.tar.gz")
start = raw.find(b"\n", idx) + 1
size = int(raw[idx + 48:idx + 58])
with tarfile.open(fileobj=io.BytesIO(raw[start:start + size]), mode="r:gz") as tar:
    control = tar.extractfile("control").read().decode()
kib = next(int(l.split(":", 1)[1]) for l in control.splitlines()
           if l.startswith("Installed-Size:"))
print(kib * 1024)
EOF
)

mkdir -p "$ROOT/api" "$ROOT/api/desc" "$ROOT/manifests"

# Rich HTML description rendered by the Homebrew Channel details screen
# (shortDescription is the only text it shows without fullDescriptionUrl).
cat > "$ROOT/api/desc/${ID}.html" <<'DESC_HTML'
<h2>Why</h2>
<p>The stock LG Magic Remote is oversized for what most TV use looks like: move focus, press OK,
press Back. The Boxput BPR1S Plus is the opposite - a small, light remote with d-pad and a few
shape buttons, comfortable in one hand. Out of the box, webOS ignores its dongle. This app maps
exactly the keys that matter (Home, OK, Back, Menu) and lets the small remote replace the big
one for daily use - same launcher, same apps, no giant remote on the couch table. The dongle's
gyro pointer keeps working untouched for the rare pointer moments.</p>
<h2>What it does</h2>
<p>The Boxput BPR1S Plus 2.4 GHz USB remote sends Home, OK, Back and Menu as
keycodes webOS ignores. This service maps them: Home and Menu open the launcher,
OK sends ENTER, Back sends the real Back key (keyCode 461, the same path LG's
phone remote uses). Every other key passes through unchanged.</p>
<h2>Requirements</h2>
<ul>
<li>Rooted LG webOS TV with <a href="https://github.com/webosbrew/webos-homebrew-channel">Homebrew Channel</a></li>
<li>32-bit soft-float userland (webOS 5 and 6). TVs with a 64-bit userland are not supported</li>
<li>Boxput BPR1S Plus with USB dongle</li>
</ul>
<h2>How it runs</h2>
<p>A single static binary, about 50 KB, one thread, <code>nice -n 19</code>. It opens only
the dongle's keyboard event nodes, read-only, and never grabs them, so the
remote keeps working when the service stops. It blocks in <code>epoll</code> and stays at
0% CPU while idle. Autostart is installed on the first launch through the
Homebrew Channel service and removed with the app.</p>
<h2>Source</h2>
<p><a href="https://github.com/neoscaler/webos-bpr1s-keymapper">github.com/neoscaler/webos-bpr1s-keymapper</a> (MIT)</p>
DESC_HTML

cat > "$ROOT/manifests/${ID}.json" <<EOF
{
  "id": "$ID",
  "version": "$VERSION",
  "type": "web",
  "title": "BPR1S Plus Key Mapper",
  "appDescription": "$APP_DESCRIPTION",
  "iconUri": "$ICON_URL",
  "sourceUrl": "https://github.com/neoscaler/webos-bpr1s-keymapper",
  "rootRequired": true,
  "ipkUrl": "$IPK_URL",
  "ipkHash": { "sha256": "$SHA" },
  "ipkSize": $SIZE,
  "installedSize": $INSTALLED_SIZE
}
EOF
MANIFEST_JSON=$(cat "$ROOT/manifests/${ID}.json")

cat > "$ROOT/api/apps.json" <<EOF
{
  "packages": [
    {
      "id": "$ID",
      "title": "BPR1S Plus Key Mapper",
      "iconUri": "$ICON_URL",
      "manifestUrl": "$PAGES/manifests/${ID}.json",
      "manifest": $MANIFEST_JSON,
      "fullDescriptionUrl": "$PAGES/api/desc/${ID}.html",
      "category": "system",
      "pool": "main",
      "shortDescription": "$SHORT_DESCRIPTION",
      "description": "$LONG_DESCRIPTION"
    }
  ]
}
EOF

case "$VERSION" in
*-*)
    { echo "<p><strong>Pre-release $VERSION.</strong> This build is under test; install it only if you want to test new features.</p>"; cat "$ROOT/api/desc/${ID}.html"; } > "$ROOT/api/desc/${ID}-beta.html"
    cat > "$ROOT/manifests/${ID}-beta.json" <<EOF
{
  "id": "$ID",
  "version": "$VERSION",
  "type": "web",
  "title": "BPR1S Plus Key Mapper",
  "appDescription": "Pre-release $VERSION: $APP_DESCRIPTION",
  "iconUri": "$ICON_URL",
  "sourceUrl": "https://github.com/neoscaler/webos-bpr1s-keymapper",
  "rootRequired": true,
  "ipkUrl": "$IPK_URL",
  "ipkHash": { "sha256": "$SHA" },
  "ipkSize": $SIZE,
  "installedSize": $INSTALLED_SIZE
}
EOF
    BETA_JSON=$(cat "$ROOT/manifests/${ID}-beta.json")
    cat > "$ROOT/api/apps-beta.json" <<EOF
{
  "packages": [
    {
      "id": "$ID",
      "title": "BPR1S Plus Key Mapper (beta $TAG)",
      "iconUri": "$ICON_URL",
      "manifestUrl": "$PAGES/manifests/${ID}-beta.json",
      "manifest": $BETA_JSON,
      "fullDescriptionUrl": "$PAGES/api/desc/${ID}-beta.html",
      "category": "system",
      "pool": "beta",
      "shortDescription": "Pre-release $VERSION of the key mapper for the Boxput BPR1S Plus remote on rooted LG webOS TVs.",
      "description": "Pre-release build $VERSION of the key mapper for the Boxput BPR1S Plus 2.4 GHz USB remote. [Source](https://github.com/neoscaler/webos-bpr1s-keymapper)"
    }
  ]
}
EOF
    ;;
*)
    rm -f "$ROOT/manifests/${ID}-beta.json" "$ROOT/api/desc/${ID}-beta.html"
    cat > "$ROOT/api/apps-beta.json" <<EOF
{
  "packages": []
}
EOF
    ;;
esac

echo "repo files: api/apps.json, api/apps-beta.json, api/desc/${ID}.html, manifests/${ID}.json (sha256=$SHA)"
