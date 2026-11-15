# Changelog

All notable changes to this project. Generated from Conventional Commits with
`git-cliff -o CHANGELOG.md`.

## v1.1.12 - 2026-11-15

### Bug fixes

- **app:** time out unanswered Homebrew Channel calls
- **packaging:** write IPK members without a ./ prefix

### Documentation

- document the heartbeat, status path and IPK layout

### Features

- **daemon:** write a status heartbeat and derive the status path
- **packaging:** add licence, 160px icon and central repo metadata

## v1.1.11 - 2026-11-15

### Features

- **packaging:** restart the daemon when the app is opened

## v1.1.10 - 2026-11-15

### Bug fixes

- **daemon:** send Back through networkinput/sendSpecialKey

## v1.1.9 - 2026-11-15

### Bug fixes

- **app:** read status.json through the Homebrew Channel exec call

### Documentation

- document the one-time launch after install

### Maintenance

- **release:** 1.1.9

## v1.1.8 - 2026-11-15

### Bug fixes

- **app:** read status.json without query string on file protocol

### Maintenance

- **release:** 1.1.8

## v1.1.7 - 2026-11-15

### Documentation

- **packaging:** state webOS 5 and 6 compatibility

### Maintenance

- **release:** 1.1.7

## v1.1.6 - 2026-11-15

### Bug fixes

- **packaging:** write IPK archives as plain ustar

### Maintenance

- **release:** 1.1.6

## v1.1.5 - 2026-11-15

### Bug fixes

- **packaging:** keep IPK payload inside usr/palm

### Maintenance

- **release:** 1.1.5

## v1.1.4 - 2026-11-15

### Features

- **packaging:** rename app to BPR1S Plus Key Mapper

### Maintenance

- **release:** 1.1.4

## v1.1.3 - 2026-11-15

### Bug fixes

- **packaging:** use non-zero timestamps in IPK archives
- **repo:** embed manifest and use Pages icon

### Maintenance

- **release:** 1.1.3

## v1.1.2 - 2026-11-15

### Bug fixes

- **packaging:** add app icon and complete repo metadata

### Maintenance

- **release:** 1.1.2

## v1.1.1 - 2026-11-15

### Documentation

- split end-user README and developer docs

### Maintenance

- **release:** 1.1.1

### Performance

- **daemon:** filter pointer nodes and dedupe status writes

## v1.1.0 - 2026-11-14

### Documentation

- translate README and info page to English

### Features

- **daemon:** --version flag and version in status.json
- **repo:** tag-pinned IPK URL and beta channel

### Maintenance

- **ci:** Actions auf aktuelle Major-Versionen
- English build messages and tag check in CI
- **release:** 1.1.0

## v1.0.0 - 2026-11-14

### Bug fixes

- **packaging:** prerm beendet nur noch den eigenen Daemon
- **packaging:** init.d-Hook mit PID-Datei, Versuchslimit und Log
- **packaging:** postinst prueft Installation und ABI
- **daemon:** nur verifizierte Dongle-Nodes beschreiben
- **daemon:** Status-Write drosseln, haengende Tasten freigeben
- **daemon:** Lock-Datei absichern, ABI-Fehler melden, --selftest
- **build:** Version nur noch aus packaging/control beziehen

### Documentation

- Installationsweg, Deinstallation und TV-Test korrigieren

### Features

- **build:** reproduzierbare IPKs
- **ci:** Workflow fuer reproduzierbare Builds und Releases

### Maintenance

- build.sh repariert (for-loop), inject-test fuer beide Targets
- **dist:** IPK und Manifest mit gefixten Quellen neu bauen
- **dist:** nur noch das IPK versionieren
- **release:** 0.2.0
- **release:** 1.0.0

### Other

- BPR1S key mapper v0.1.0: daemon (C, static musl, epoll, uinput), uinput-test prototype, IPK packaging with webosbrew init.d hook
- Anleitung Daemon-Test auf dem TV (uinput erst nach Reboot wirksam?)
- Robuste Geraeteerkennung: sysfs-Fallback (/sys/class/input), Matching ueber Name oder Vendor/Product, Selbst-Aussschluss des uinput-Geraets
- Puffer fuer sysfs-Pfade passend erhoeht (saubere -Wall-Warnungen)
- Vendor/Product dezimal (4842) und hex (12ea) matchen; README korrigiert
- uinput raus (webOS 5 ignoriert virtuelle uinput-Events), OK/Zurueck ueber webOS Pointer-Socket (unix socket, type:button/name:ENTER|BACK); pointer-test als TV-Test; Doku aktualisiert
- Injection per write() in den Dongle-Tastatur-Node (O_RDWR, leds-Handler) statt uinput/Pointer-Socket; Echo-Unterdrueckung fuer KEY_BACK; pointer-* entfernt
- inject-test <keycode>: einzelner Tastendruck in den Dongle-Node (zum Ermitteln des webOS-Back-Codes)
- Zurueck mappt auf KEY_ESC (webOS wertet KEY_BACK vom Dongle nicht aus; ESC/BACKSPACE getestet: ESC = kanonisches Back)
- Generisches ACT_LAUNCH <app-id>: Menue-Taste (KEY_COMPOSE 0x7f) startet konfigurierbare App (Default com.webos.app.home = Launcher/Recents)
- Info-Seite (index.html + status.json, Daemon schreibt Adapter-Status) und Custom-Repo-Dateien (api/apps.json + manifests/*.json mit ipkUrl+sha256)
- Single-Instance per flock (/tmp/bpr1s-keymapper.lock) - verhindert doppelte Injektion bei parallelen Daemonen


