# BPR1S Plus Key Mapper

Homebrew app for rooted LG webOS TVs (tested on LG OLED65CX, webOS 5, kernel 4.4.84,
32-bit soft-float userland). It makes the keys webOS ignores usable on the **Boxput BPR1S Plus**
2.4 GHz USB dongle (vendor 0x12ea / 4842, product 0x0001).

## Key mapping

| Key | Keycode | Action |
|---|---|---|
| Home | `KEY_HOMEPAGE` (0xac) | opens `com.webos.app.home` (with Luma Launcher Home takeover: the launcher, like the original Home key) |
| OK (center of the d-pad) | `KEY_SELECT` (0x161) | `KEY_ENTER`, press/release/repeat 1:1 |
| Back | `KEY_BACK` (0x9e) | `KEY_ESC` (webOS handles ESC as back and ignores KEY_BACK) |
| Menu (square) | `KEY_COMPOSE` (0x7f) | opens `com.webos.app.home` (launcher/recents with Luma Home takeover) |
| Microphone | — | no function (webOS ignores it) |

All other keys reach webOS unchanged. The daemon opens the dongle's event nodes read-only
and never grabs them, so the remote keeps working even if the daemon stops.

## Requirements

- LG webOS TV, rooted, [Homebrew Channel](https://github.com/webosbrew/webos-homebrew-channel) installed
- 32-bit soft-float userland (webOS TV standard). The IPK ships only the `abi` build; TVs with
  `armhf` userland are not supported
- Boxput BPR1S Plus with USB dongle

## Install

1. In the Homebrew Channel open **Settings → Add repository** and add:
   `https://neoscaler.github.io/webos-bpr1s-keymapper/api/apps.json`
2. Install **BPR1S Plus Key Mapper** from the app list. Root is required; the IPK hash is verified.
3. Autostart is installed. The daemon starts immediately when webOS is already running; otherwise
   it starts on the next boot, so a reboot is only needed in the latter case.

Do not install the IPK with `ipkg`/`opkg` by hand: the payload path `/usr/palm/applications/...`
then lands on the system partition instead of the app directory under `/media/developer/apps`.
`postinst` aborts in that case and leaves no autostart entry behind.

## Uninstall

Uninstall via the Homebrew Channel. This removes the app directory and the init.d hook and
stops the daemon, including its restart loop. A short log of the last starts stays in
`/tmp/bpr1s-keymapper.log` until the next reboot.

## Status page

The app has a small status page. It shows the running version, whether the daemon is running,
whether the **Boxput BPR1S Plus adapter was found** (including the write target node), the last mapped key
and the event count. The page only polls while it is open (every 2 s).

## Resource usage

The daemon is deliberately small and passive:

- Static binary of about 50 KB, RSS about 90 KB, one thread, `nice -n 19` (measured with the
  same code on x86_64 musl; ARM values are in the same range).
- Blocks in `epoll`. No polling, no timers: 0 % CPU while idle.
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
- Tested on webOS 5 (LG OLED65CX). Other rooted LG webOS TVs with a 32-bit soft-float userland
  may work. The mapping table can be adapted for other remotes; see [DEVELOPING.md](DEVELOPING.md).
- Home and Menu call `com.webos.applicationManager` and assume the standard webOS application
  manager is present.
- No system partition is touched. Persistent changes are the app directory under
  `/media/developer/apps` and the init.d hook; both are removed on uninstall. Runtime files are
  `/tmp/bpr1s-keymapper.lock`, `/tmp/bpr1s-keymapper.loop.pid`,
  `/tmp/bpr1s-keymapper.log` and the atomically replaced `status.json` in the app directory.
