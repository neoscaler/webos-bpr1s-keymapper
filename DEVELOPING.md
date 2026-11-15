# Developing

Developer documentation for the BPR1S Plus Key Mapper. End-user installation is covered in
[README.md](README.md).

## Build (developer PC, Linux x86_64)

```sh
git clone https://github.com/neoscaler/webos-bpr1s-keymapper.git
cd webos-bpr1s-keymapper
tools/get-zig.sh           # once: downloads Zig 0.14.1 to tools/zig, checksum verified
tools/build.sh             # dist/bpr1s-keymapper.{abi,abihf} (static, musl)
tools/build-ipk.sh abi     # dist/com.neoscaler.bpr1s-keymapper_<version>_all.ipk
```

The version is defined in `packaging/control`; `build-ipk.sh` copies it into `appinfo.json`
and the manifest files. In Git, `dist/` contains only the IPK. Binaries, staging and tarballs
are build output and are ignored.

The TV userland is **soft-float** (`/lib/ld-linux.so.3`, e_flags `0x05000200`), so use the
`abi` binary (the default in `build-ipk.sh`). Use `abihf` only for TVs with
`ld-linux-armhf.so.3` / e_flags bit `0x400`.

## Tests on the TV (before installing)

```sh
# PC:  tools/build.sh, then scp dist/bpr1s-keymapper.abi to the TV, e.g. /media/developer/km
# TV:
chmod +x /media/developer/km
/media/developer/km --verbose 2>/tmp/km.log &
# BPR1S Plus: does OK open a menu item? Does Home open the launcher? Does Back navigate back?
# T4: open ARD or ZDF, press Back until the app's start page -> Back must work there too
#     (that is the case ESC never covered), then check Netflix and the webOS menus
# T5: open YouTube/Netflix, press Home
cat /tmp/km.log
```

`--version` prints the build version, `--selftest` exits 0 without touching any device.

## How it works (KISS)

A single static C file (`src/bpr1s-keymapper.c`, no dependencies beyond libc): device
discovery (`/proc/bus/input/devices`, sysfs fallback) -> `epoll` on the dongle interfaces ->
fixed mapping table -> `fork`/`exec` of `/usr/bin/luna-send` (Home/Menu/Back) or `write()` of
input_events into the dongle keyboard node (OK). Hotplug via HUP/ENODEV with a rescan
every 10 s while no dongle is found. `nice -n 19`, `SIGCHLD` ignored (no zombies), no logging
unless `--verbose`. The write path is verified by vendor/product; a name match is only used
when the IDs are unreadable, and the sysfs fallback never writes. Held keys are released on
device loss and shutdown, so a crash cannot leave a key stuck down.

Only read-only event nodes that report at least one mapped key are opened. The filter uses
`EVIOCGBIT` and drops pointer-only nodes, so the mouse/gyro interface causes no wakeups. The
scheduled write target (detected by the `leds` handler) is always opened.

Injected keys are written as `input_event` structs (2 x 16 bytes: EV_KEY + EV_SYN) with
`write()` directly into the **dongle's keyboard node** (O_RDWR). The kernel distributes them
to all readers of that device, including webOS/surface-manager, which already reads that node.
It is the same node the original keys arrive on, so webOS accepts the events. **uinput is not
used**: on webOS 5, events created virtually via uinput are ignored (tested). Injected events
are suppressed on the write fd with an echo counter (otherwise the injected `KEY_ENTER` would
feed back into itself). Only OK uses this path.

Back cannot be injected. The original remote's Back arrives as evdev 412 (`KEY_PREVIOUS`) and
webOS turns it into keyCode 461, which apps handle. Neither `KEY_BACK` (158, the dongle's own
code) nor `KEY_ESC` (1) reaches apps as 461: ESC arrives as keyCode 27, which only some apps
treat as Back (Netflix, the webOS UI) and others do not (ARD, ZDF). Back therefore calls
`com.webos.service.networkinput/sendSpecialKey` with `{"key":"BACK"}`, the same path LG's phone
remote uses. `luna-send` needs stdin on `/dev/null`; otherwise it can return without delivering.

## Debugging

Manual start with verbose logging:

```sh
/media/developer/apps/usr/palm/applications/com.neoscaler.bpr1s-keymapper/bin/bpr1s-keymapper --verbose
```

`BPR1S_STATUS_PATH` overrides the `status.json` location (default:
`/media/developer/apps/usr/palm/applications/com.neoscaler.bpr1s-keymapper/status.json`).

Resource usage on the target can be checked without stopping the service:

```sh
pid=$(pidof bpr1s-keymapper)
awk '{print "utime="$14, "stime="$15}' /proc/$pid/stat
grep voluntary_ctxt_switches /proc/$pid/status
top -b -n1 -p "$pid"
# repeat after 60 s with the remote untouched and while moving it
```

## Custom Homebrew repository

`tools/build-ipk.sh` regenerates `api/apps.json`, `api/apps-beta.json` and
`manifests/<id>.json` with the current version and SHA-256. The public GitHub Pages endpoint
used in the README is:

```text
https://neoscaler.github.io/webos-bpr1s-keymapper/api/apps.json
```

Development variants:

- **without Pages** (raw URLs):
  `https://raw.githubusercontent.com/neoscaler/webos-bpr1s-keymapper/main/api/apps.json`
- **beta channel** (pre-releases, same URL with `api/apps-beta.json`; empty unless the
  current version is a pre-release such as `1.1.0-beta.1`)

The IPK URL points at the Git tag (`.../v<version>/dist/...`), so a published version stays
installable after a newer one lands.

## Release

1. Bump the version in `packaging/control` (SemVer) and mirror it in `packaging/appinfo.json`.
2. `tools/build.sh && tools/build-ipk.sh abi` — builds the IPK, regenerates `api/*.json`
   and `manifests/*.json` including SHA-256, removes older IPKs from `dist/`.
3. Update `CHANGELOG.md` (`git-cliff -o CHANGELOG.md`), commit, tag `v<version>`, push branch and tag.

The tag push starts `.github/workflows/build.yml`: it builds the binaries with the pinned
Zig version, checks the manifest hash against the IPK, compares the IPK binary with the
build from source, checks the tag against `packaging/control`, and creates the release with
the IPK as asset. Builds are reproducible (`tools/pack-ipk.py`, fixed timestamps), so the
committed artifact can always be verified against the sources.

## IPK format

`tools/pack-ipk.py` writes the IPK by hand. Constraints from webOS 5, all of them verified on
the target TV:

- `data.tar.gz` and `control.tar.gz` must be plain ustar with `./`-prefixed paths. Members
  produced by GNU `tar` are rejected with `-5 ipk verified failed`.
- Timestamps must not be epoch 0. `pkgverifier` treats 1970 as "implausibly old".
- The payload may only contain `usr/palm/**`. Files outside it, such as a prebuilt
  `var/lib/webosbrew/init.d` hook, fail verification.
- `control.tar.gz` contains only `./control`. Control scripts are not shipped: LG installs
  IPKs without executing `postinst`/`prerm`.

Autostart therefore comes from the app itself. On launch the status page calls
`luna://org.webosbrew.hbchannel.service/exec` and copies `bin/60-bpr1s-keymapper` into
`/var/lib/webosbrew/init.d`.

## Status page

webOS loads the app from `file://`, where `XMLHttpRequest` is blocked (status 0) and a query
string becomes part of the file name. The page therefore reads `status.json` through the same
Homebrew Channel `exec` call (`cat …/status.json`), polled every 3 s. The XHR path stays for
serving the page over `http`. Without the Homebrew Channel service the page cannot show the
daemon state at all.
