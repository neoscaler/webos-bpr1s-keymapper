#!/bin/sh
# TV-seitige Tests fuer den BPR1S Key Mapper. Auf dem TV als root via SSH ausfuehren:
#   ssh root@<tv> -p 9922 'sh -s' < tools/tv-test.sh
echo "== Kernel / Userland =="
uname -r
uname -m
getconf LONG_BIT
echo
echo "== T1: ABI von /bin/busybox (e_flags, Bit 0x400 = Hard-Float) =="
od -An -tx4 -j36 -N4 /bin/busybox
ls -l /lib/ld-linux* 2>/dev/null
echo
echo "== T0: luna-send Pfad =="
which luna-send
echo
echo "== uinput Node =="
ls -l /dev/uinput /dev/misc/uinput 2>/dev/null
echo
echo "== Dongle in /proc/bus/input/devices =="
grep -B1 -A6 -i 'vendor=12ea' /proc/bus/input/devices
echo
echo "== Naechste manuelle Schritte =="
echo "T4: App oeffnen (z.B. YouTube), dann:"
echo "  luna-send -n 1 luna://com.webos.applicationManager/launch '{\"id\":\"com.webos.app.home\"}'"
echo "  -> erscheint der Luma Launcher?"
echo "T3: in ein Untermenue navigieren, Zurueck-Taste der BPR1S druecken -> geht sie von allein?"
echo "T2: uinput-test vom PC nach /tmp scp'en, dann auf dem TV:"
echo "  chmod +x /tmp/uinput-test.abi && /tmp/uinput-test.abi"
echo "  (mit den BPR1S-Pfeiltasten vorher einen Menuepunkt fokussieren; nach 5 s wird Enter gesendet)"
