# BPR1S Key Mapper

Homebrew-App fuer gerootete LG webOS TVs (getestet: LG OLED65CX, webOS 5), die die von
webOS ignorierten Tasten der **Boxput BPR1S Plus** (2,4-GHz-USB-Dongle, Vendor `4842`/0x12ea,
Product `0001`) nutzbar macht:

| Taste | Keycode | Aktion |
|---|---|---|
| Home | `KEY_HOMEPAGE` (0xac) | `luna-send` -> `launch com.webos.app.home` (öffnet bei Luma Launcher mit Home Takeover den Launcher, wie die Original-Home-Taste) |
| OK (Steuerkreuz Mitte) | `KEY_SELECT` (0x161) | `KEY_ENTER` wird 1:1 ueber ein virtuelles uinput-Keyboard gespiegelt |
| Zurueck | `KEY_BACK` (0x9e) | `KEY_BACK` wird 1:1 per uinput gespiegelt (auf dem TV getestet: webOS wertet das originale Event nicht aus, daher keine Doppel-Ausloesung) |
| Menue, Mikrofon | — | v1: unveraendert durchgereicht (webOS ignoriert sie) |

Alle anderen Tasten kommen weiterhin normal bei webOS an: Der Daemon oeffnet die
Event-Devices des Dongles **lesend ohne EVIOCGRAB** und liest per `epoll` blockierend —
im Leerlauf 0 % CPU, kein Polling. Der Maus-/Gyro-Strom auf dem zweiten Interface wird
anhand des Event-Typs verworfen (nur `EV_KEY` wird ausgewertet). Event-Nummern werden zur
Laufzeit ueber `/proc/bus/input/devices` anhand Vendor/Product gefunden (Hotplug-sicher);
beim Abziehen wird automatisch neu gesucht.

Das virtuelle uinput-Keyboard wird beim Start angelegt und beim Beenden automatisch
entfernt. Ausser dem App-Verzeichnis und einer init.d-Hook-Datei werden **keine** Systemeinstellungen
geaendert; die Deinstallation entfernt beides restlos.

## Voraussetzungen

- LG webOS TV, gerootet, [Homebrew Channel](https://github.com/webosbrew/webos-homebrew-channel) installiert
- 32-Bit-Userland (webOS TV Standard), aarch64-Kernel
- Boxput BPR1S Plus mit USB-Dongle

## Build (Entwickler-PC, Linux x86_64)

```sh
git clone https://github.com/neoscaler/webos-bpr1s-keymapper.git
cd webos-bpr1s-keymapper
tools/get-zig.sh           # einmalig: laedt Zig 0.14.1 nach tools/zig (curl/wget/python3)
tools/build.sh             # dist/bpr1s-keymapper.{abi,abihf} + dist/uinput-test.{abi,abihf} (statisch, musl)
tools/build-ipk.sh abi     # dist/com.neoscaler.bpr1s-keymapper_0.1.0_all.ipk
```

Wichtig: Das TV-Userland ist **Soft-Float** (`/lib/ld-linux.so.3`, e_flags `0x05000200`),
daher das `abi`-Binary verwenden (Default in `build-ipk.sh`). `abihf` nur fuer TVs mit
`ld-linux-armhf.so.3` / e_flags-Bit `0x400`.

## Tests auf dem TV (vor der Installation)

```sh
ssh root@<tv> -p 9922 'sh -s' < tools/tv-test.sh     # T0/T1 + Geraeteuebersicht
scp dist/uinput-test.abi root@<tv>:/tmp/              # T2: nimmt webOS uinput-Events an?
```

T2-Detail: Auf dem TV `/tmp/uinput-test.abihf` starten, mit den BPR1S-Pfeiltasten einen
Menuepunkt fokussieren — nach 5 s sendet der Prototyp einmal Enter; der fokussierte Eintrag
muss sich oeffnen. Ausserdem manuell testen: T3 (Zurueck-Taste in einem Untermenue) und
T4 (`luna-send ... launch com.webos.app.home` waehrend eine App laeuft).

## Installation / Deinstallation

IPK ueber den Homebrew Channel installieren (oder `ares-install`). Autostart ueber
`/var/lib/webosbrew/init.d/60-bpr1s-keymapper` (wird vom Homebrew Channel beim Boot als
root ausgefuehrt; die Datei liegt im IPK-Payload und wird von `prerm` bei der Deinstallation
entfernt). Sofortiger Start ohne Reboot: `postinst` startet den Hook, sobald webOS bereits
gebootet ist.

Deinstallation ueber den Homebrew Channel — App-Verzeichnis, init.d-Hook und laufender
Daemon werden entfernt (`prerm` killt den Prozess).

Manueller Start zum Debuggen: `.../bin/bpr1s-keymapper --verbose`

## Funktionsweise (KISS)

Eine einzige statische C-Datei (`src/bpr1s-keymapper.c`, ~250 Zeilen, keine Abhaengigkeiten
ausser libc): Device-Discovery -> `epoll` auf beide Dongle-Interfaces -> feste Mapping-Tabelle
-> `fork`/`exec` von `/usr/bin/luna-send` (Home) bzw. uinput-Injektion (OK). Hotplug ueber
HUP/ENODEV mit erneutem Scan alle 10 s, solange kein Dongle gefunden wird. `nice -n 19`,
`SIGCHLD` ignoriert (keine Zombies), kein Logging (ausser `--verbose` auf stderr).

## Kompatibilitaet

Getestet auf webOS 5 (LG OLED65CX). Prinzipiell geeignet fuer andere gerootete LG webOS TVs
mit 32-Bit-Userland und `/dev/uinput`; die Mapping-Tabelle in `src/bpr1s-keymapper.c` laesst
sich fuer andere Remotes anpassen (Vendor/Product oben aendern).

## English summary

Root-only Homebrew Channel app for LG webOS TVs that maps the unused Home/OK buttons of the
Boxput BPR1S Plus 2.4 GHz remote: Home launches `com.webos.app.home` via `luna-send`, OK is
mirrored as `KEY_ENTER` through a virtual uinput keyboard. Passive read-only listener (no
`EVIOCGRAB`), epoll-based, zero idle CPU, hotplug-safe, fully removed on uninstall.
