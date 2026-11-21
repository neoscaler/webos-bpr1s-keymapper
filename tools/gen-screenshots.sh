#!/bin/sh
# Renders screenshots of the status page UI by feeding the real index.html
# markup with static status values and capturing it in Chromium headless at
# TV resolution. Output: screenshots/status.png, screenshots/keymapping-saved.png
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VIEWPORT=1920x1080
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

python3 - "$ROOT" "$TMP" <<'EOF'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
tmp = pathlib.Path(sys.argv[2])
html = (root / "packaging" / "index.html").read_text()
# Static fixtures: the markup and CSS of the real page without its JS.
html = re.sub(r"<script>.*</script>", "", html, flags=re.S)


def select(browser_id, label, focus=False):
    cls = "focus" if focus else ""
    return (f'<select id="{browser_id}" class="{cls}">'
            f'<option selected>{label}</option></select>')


STATUS = [
    ('<span class="value" id="version">&hellip;</span>',
     '<span class="value" id="version">1.2.5</span>'),
    ('<span id="autostart" class="pill">&hellip;</span>',
     '<span id="autostart" class="pill ok">installed</span>'),
    ('<span id="running" class="pill">&hellip;</span>',
     '<span id="running" class="pill ok">yes</span>'),
    ('<span id="dongle" class="pill">&hellip;</span>',
     '<span id="dongle" class="pill ok">found</span>'),
    ('<span class="value small" id="target">&hellip;</span>',
     '<span class="value small" id="target">/dev/input/event5</span>'),
    ('<span class="value" id="lastkey">&hellip;</span>',
     '<span class="value" id="lastkey">HOME</span>'),
    ('<span class="value" id="events">&hellip;</span>',
     '<span class="value" id="events">1287</span>'),
]

out = html
for old, new in STATUS:
    out = out.replace(old, new)
for browser_id, label, focus in [
    ("sel-square", "Cycle HDMI 1-4", True),
    ("sel-triangle", "Home", False),
    ("sel-circle", "Live TV", False),
]:
    out = re.sub(rf'<select id="{browser_id}"[^>]*></select>',
                 lambda m, b=browser_id, l=label, f=focus: select(b, l, f), out)
(tmp / "status.html").write_text(out)

for browser_id, label in [("sel-circle", "Netflix")]:
    out = re.sub(rf'<select id="{browser_id}"[^>]*>.*?</select>',
                 lambda m, b=browser_id, l=label: select(b, l), out)
out = out.replace('<span id="mapmsg"></span>', '<span id="mapmsg" class="ok">saved</span>')
(tmp / "keymapping-saved.html").write_text(out)
EOF

cp "$ROOT/packaging/icon80.png" "$TMP/"
mkdir -p "$ROOT/screenshots"

which chromium >/dev/null 2>&1 || { echo "chromium not in PATH (nix shell nixpkgs#chromium)" >&2; exit 1; }
for name in status keymapping-saved; do
    chromium --headless=new --disable-gpu --no-first-run --hide-scrollbars \
        --window-size=$VIEWPORT \
        --screenshot="$ROOT/screenshots/$name.png" \
        "file://$TMP/$name.html" 2>/dev/null
    echo "screenshots/$name.png"
done
