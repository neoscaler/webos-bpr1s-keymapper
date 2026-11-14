# BPR1S Key Mapper

Homebrew-App fuer gerootete LG webOS TVs (getestet: LG OLED65CX, webOS 5, Kernel 4.4.84,
32-Bit-Userland soft-float), die die von webOS ignorierten Tasten der **Boxput BPR1S Plus**
(2,4-GHz-USB-Dongle, Vendor 4842/0x12ea, Product 0001) nutzbar macht:

| Taste | Keycode | Aktion |
|---|---|---|
| Home | `KEY_HOMEPAGE` (0xac) | `luna-send` -> `launch com.webos.app.home` (öffnet bei Luma Launcher mit Home Takeover den Launcher, wie die Original-Home-Taste) |
| OK (Steuerkreuz Mitte) | `KEY_SELECT` (0x161) | `KEY_ENTER` wird per `write()` in den Tastatur-Node des Dongles eingespeist (Druck/Loslassen/Repeat 1:1) |
| Zurueck | `KEY_BACK` (0x9e) | `KEY_ESC` wird eingespeist (auf dem TV getestet: webOS wertet KEY_BACK nicht aus, ESC/BACKSPACE schon — ESC ist das kanonische webOS-Back) |
| Menue, Mikrofon | — | v1: unveraendert durchgereicht (webOS ignoriert sie) |

Alle anderen Tasten kommen weiterhin normal bei webOS an: Der Daemon oeffnet die
Event-Devices des Dongles **lesend ohne EVIOCGRAB** und liest per `epoll` blockierend —
im Leerlauf 0 % CPU, kein Polling. Der Maus-/Gyro-Strom auf dem zweiten Interface wird
anhand des Event-Typs verworfen (nur `EV_KEY` mit Wert 1 wird ausgewertet). Event-Nummern
werden zur Laufzeit gefunden (Vendor/Product dezimal *und* hex, dazu Name-Match, mit
Fallback auf sysfs `/sys/class/input`), da sie bei jedem Einstecken wechseln.

Die Tastendruecke werden als `input_event`-Structs (2 x 16 Byte: EV_KEY + EV_SYN) per
`write()` direkt in den **Tastatur-Node des Dongles** geschrieben (O_RDWR; erkannt am
`leds`-Handler). Der Kernel verteilt sie an alle Leser dieses Geraets — also auch an
webOS/surface-manager, das diesen Node ohnehin liest. Das ist derselbe Node, auf dem die
Originaltasten ankommen, daher akzeptiert webOS die Events. **uinput wird nicht verwendet**
— auf webOS 5 werden virtuell per uinput erzeugte Events ignoriert (getestet). Eigene
injizierte Events werden am Schreib-FD per Echo-Counter unterdrueckt (sonst Rueckkopplung
bei KEY_BACK).

Ausser dem App-Verzeichnis und einer init.d-Hook-Datei werden **keine** persistenten
Aenderungen vorgenommen; die Deinstallation entfernt beides restlos.

## Voraussetzungen

- LG webOS TV, gerootet, [Homebrew Channel](https://github.com/webosbrew/webos-homebrew-channel) installiert
- Boxput BPR1S Plus mit USB-Dongle

## Build (Entwickler-PC, Linux x86_64)

```sh
git clone https://github.com/neoscaler/webos-bpr1s-keymapper.git
cd webos-bpr1s-keymapper
tools/get-zig.sh           # einmalig: laedt Zig 0.14.1 nach tools/zig (curl, wget oder python3)
tools/build.sh             # dist/bpr1s-keymapper.{abi,abihf} (statisch, musl)
tools/build-ipk.sh abi     # dist/com.neoscaler.bpr1s-keymapper_0.1.0_all.ipk
```

Wichtig: Das TV-Userland ist **Soft-Float** (`/lib/ld-linux.so.3`, e_flags `0x05000200`),
daher das `abi`-Binary verwenden (Default in `build-ipk.sh`). `abihf` nur fuer TVs mit
`ld-linux-armhf.so.3` / e_flags-Bit `0x400`.

## Tests auf dem TV (vor der Installation)

```sh
# PC:  wget dist/bpr1s-keymapper.abi (raw.githubusercontent.com/neoscaler/webos-bpr1s-keymapper/main/dist/...)
#      und scp auf den TV, z. B. nach /media/developer/km
# TV:
chmod +x /media/developer/km
/media/developer/km --verbose 2>/tmp/km.log &
# BPR1S: OK oeffnet Menuepunkt? Home oeffnet Launcher? Zurueck navigiert zurueck?
# T4: YouTube/Netflix offen, Home druecken
cat /tmp/km.log
```

## Installation / Deinstallation

IPK ueber den Homebrew Channel installieren (oder `ares-install`). Autostart ueber
`/var/lib/webosbrew/init.d/60-bpr1s-keymapper` (Dateiname ohne Punkt — `run-parts`-Regel;
wird vom Homebrew Channel beim Boot als root ausgefuehrt; die Datei liegt im IPK-Payload
und wird von `prerm` bei der Deinstallation entfernt). Sofortiger Start ohne Reboot:
`postinst` startet den Hook, sobald webOS bereits gebootet ist.

Deinstallation ueber den Homebrew Channel — App-Verzeichnis, init.d-Hook und laufender
Daemon werden entfernt (`prerm` killt den Prozess).

Manueller Start zum Debuggen: `.../bin/bpr1s-keymapper --verbose`

## Funktionsweise (KISS)

Eine einzige statische C-Datei (`src/bpr1s-keymapper.c`, keine Abhaengigkeiten ausser
libc): Device-Discovery (`/proc/bus/input/devices`, Fallback sysfs) -> `epoll` auf die
Dongle-Interfaces -> feste Mapping-Tabelle -> `fork`/`exec` von `/usr/bin/luna-send`
(Home) bzw. `write()` von input_events in den Dongle-Tastatur-Node (OK/Zurueck).
Hotplug ueber HUP/ENODEV mit erneutem Scan alle 10 s, solange kein Dongle gefunden wird.
`nice -n 19`, `SIGCHLD` ignoriert (keine Zombies), kein Logging (ausser `--verbose`).

## Kompatibilitaet

Getestet auf webOS 5 (LG OLED65CX, Kernel 4.4.84). Prinzipiell geeignet fuer andere
 gerootete LG webOS TVs mit 32-Bit-Userland; die Mapping-Tabelle in
`src/bpr1s-keymapper.c` laesst sich fuer andere Remotes anpassen.

## English summary

Root-only Homebrew Channel app for LG webOS TVs that maps the unused Home/OK/Back buttons
of the Boxput BPR1S Plus 2.4 GHz remote: Home launches `com.webos.app.home` via `luna-send`,
OK/Back are injected as native input events by writing them into the dongle's own
keyboard event node (the device webOS already reads).
Passive read-only listener (no `EVIOCGRAB`), epoll-based, zero idle CPU, hotplug-safe,
fully removed on uninstall.
