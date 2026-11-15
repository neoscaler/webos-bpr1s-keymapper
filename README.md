# BPR1S Plus Key Mapper

Homebrew app for rooted LG webOS TVs (tested on LG OLED65CX, webOS 5, kernel 4.4.84,
32-bit soft-float userland). It makes the keys webOS ignores usable on the **Boxput BPR1S Plus**
2.4 GHz USB dongle (vendor 0x12ea / 4842, product 0x0001).

## Key mapping

| Button | Default | Configurable |
|---|---|---|
| Square (MENU) | cycles HDMI 1 → 2 → 3 → 4 | yes |
| Triangle (HOME) | opens `com.webos.app.home` (with Luma Launcher Home takeover: the launcher, like the original Home key) | yes |
| Circle (MIC) | opens Live TV (`com.webos.app.livetv`) | yes |
| OK (center of the d-pad) | `KEY_ENTER`, press/release/repeat 1:1 | no |
| Back | `luna-send` → `com.webos.service.networkinput/sendSpecialKey` with `{"key":"BACK"}` (the path LG's phone remote uses, so apps get the real Back keyCode 461) | no |

All other keys reach webOS unchanged. The daemon opens the dongle's event nodes read-only
and never grabs them, so the remote keeps working even if the daemon stops.

The three buttons above the d-pad send keycodes webOS ignores: `KEY_COMPOSE` (127),
`KEY_HOMEPAGE` (172) and `KEY_VOICECOMMAND` (582). They are free to use, which is why they
are the configurable ones.

Back is not sent as an evdev key. The original remote's Back arrives as evdev 412
(`KEY_PREVIOUS`) and webOS turns that into keyCode 461, which is what apps handle. Neither
`KEY_BACK` (158, what the dongle sends) nor `KEY_ESC` (1) is mapped to that: ESC only reaches
apps as keyCode 27, which some apps treat as Back (Netflix, the webOS UI) and others do not
(ARD, ZDF). Back therefore goes through the same Luna call as LG's phone remote.

## Requirements

- LG webOS TV, rooted, [Homebrew Channel](https://github.com/webosbrew/webos-homebrew-channel) installed
- 32-bit soft-float userland (webOS TV standard). The IPK ships only the `abi` build; TVs with
  `armhf` userland are not supported
- Boxput BPR1S Plus with USB dongle

## Install

1. In the Homebrew Channel open **Settings → Add repository** and add:
   `https://neoscaler.github.io/webos-bpr1s-keymapper/api/apps.json`
2. Install **BPR1S Plus Key Mapper** from the app list. Root is required; the IPK hash is verified.
3. Press **Launch** in the Homebrew Channel (or start the app from the launcher) once. webOS runs no
   install scripts, so the package itself cannot create the autostart hook; the app does it on its
   first start through the Homebrew Channel service, which runs as root. It symlinks the hook
   into `/var/lib/webosbrew/init.d` and starts the daemon. From then on the daemon starts with
   every boot. The page shows `Autostart: installed` when that worked.

webOS installs IPKs without running control scripts, so the hook cannot be created by the package
itself. The app does it through the Homebrew Channel service, which runs as root. If the Homebrew
Channel is not running as root, the page says so and the daemon only runs while started by hand.

## Update

Install the new version in the Homebrew Channel, then open the app once. It refreshes the hook and
restarts the daemon, so the new version runs without a reboot. The status page shows the running
version, which is the quickest way to confirm the update took effect.

Version 1.2.0 changed the default mapping: the square button now cycles through the HDMI
inputs, the triangle opens Home and the circle opens Live TV. Before that, square and Home both
opened the launcher. Adjust it on the status page if you prefer the old behaviour.

## Uninstall

Uninstall via the Homebrew Channel. That removes the app directory, and the hook is a symlink
into it, so it disappears with the app. To remove the key map as well:

```sh
rm -rf /var/lib/webosbrew/bpr1s-keymapper
```

A short log of the last starts stays in `/tmp/bpr1s-keymapper.log` until the next reboot.

## Status page

The app has a small status page. It shows the running version, whether the daemon is running,
whether the **Boxput BPR1S Plus adapter was found** (including the write target node), the last mapped key
and the event count. The page only polls while it is open (every 2 s).

The daemon refreshes `status.json` every 30 s, so a killed daemon cannot leave the page
showing a stale "running: yes": the page treats a heartbeat older than 90 s as stopped.

## Changing the key mapping

The page has a **Key mapping** section: one dropdown per shape button, **Save** and
**Reset to default**. Saving writes the map and reloads the daemon through `SIGHUP`, so the
new mapping applies without a restart.

The page works with the remote: up/down picks a row, left/right changes the value, OK on Save
or Reset applies it. The gyro pointer works as well.

The map lives in `/var/lib/webosbrew/bpr1s-keymapper/keymap.conf` and survives app updates:

```
# BPR1S Plus Key Mapper - key map
# buttons: square (MENU), triangle (HOME), circle (MIC)
# actions: none | launch:<app-id> | key:<NAME> | cycle:<id>,<id>,...
square   = cycle:com.webos.app.hdmi1,com.webos.app.hdmi2,com.webos.app.hdmi3,com.webos.app.hdmi4
triangle = launch:com.webos.app.home
circle   = launch:com.webos.app.livetv
```

- `none` — the button does nothing.
- `launch:<app-id>` — starts an app, e.g. `launch:netflix` or `launch:com.webos.app.browser`.
- `key:<NAME>` — sends a key through `networkinput/sendSpecialKey`. Allowed: `BACK`, `INFO`,
  `MENU`, `EXIT`, `GUIDE`, `CC`, `RED`, `GREEN`, `YELLOW`, `BLUE`, `PLAY`, `PAUSE`, `STOP`,
  `REWIND`, `FASTFORWARD`, `CHANNELUP`, `CHANNELDOWN`.
- `cycle:<id>,<id>,...` — starts the next app in the list on every press (up to four).

A held button repeats `key:` actions at most every 150 ms; `launch:` and `cycle:` fire once
per press. A rejected line leaves that button at its default and the page shows the reason
under the mapping table. Deleting the file restores the defaults.

## Resource usage

The daemon is deliberately small and passive:

- Static binary of about 50 KB, RSS about 90 KB, one thread, `nice -n 19` (measured with the
  same code on x86_64 musl; ARM values are in the same range).
- Blocks in `epoll`. No polling: 0 % CPU while idle. One `timerfd` wakeup every 30 s writes
  the status heartbeat; nothing else runs on a timer.
- Only evdev nodes that report at least one mapped key are opened (`EVIOCGBIT` filter). The
  pointer/gyro interface of the dongle is filtered out, so moving the remote or gyro drift
  causes no wakeups.
- Hotplug rescan every 10 s, only while the dongle is missing.
- `status.json` is replaced atomically, at most every 2 s and only when its content changes.
  No logging unless `--verbose` is passed.
- No `EVIOCGRAB`: a stopped daemon never blocks the remote.

## Compatibility and safety

- Requires root. This is an unofficial project, not affiliated with LG or Boxput. Use at your
  own risk and re-check it after webOS updates.
- Tested on webOS 5 (LG OLED65CX). webOS 6 on rooted TVs with 32-bit soft-float userland is
  supported by the same package. The IPK ships only the 32-bit `abi` build; TVs with an
  `aarch64` userland are not supported.
- The year-based names (webOS 25, webOS 26) are webOS 10 and 11. The package format is
  compatible, but those models usually need root first (often unavailable on current
  firmware), and most have a 64-bit userland, which this build does not cover.
- Other rooted LG webOS TVs with a 32-bit soft-float userland may work. The mapping table can
  be adapted for other remotes; see [DEVELOPING.md](DEVELOPING.md).
- Home and Menu call `com.webos.applicationManager`, Back calls
  `com.webos.service.networkinput/sendSpecialKey`; both assume the standard webOS services are
  present.
- No system partition is touched. Persistent changes are the app directory under
  `/media/developer/apps`, the init.d hook in `/var/lib/webosbrew/init.d` and the key map in
  `/var/lib/webosbrew/bpr1s-keymapper`. Runtime files are
  `/tmp/bpr1s-keymapper.lock`, `/tmp/bpr1s-keymapper.loop.pid`,
  `/tmp/bpr1s-keymapper.log` and the atomically replaced `status.json` in the app directory.
