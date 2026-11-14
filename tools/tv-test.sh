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
echo "== luna-send Pfad =="
which luna-send
echo
echo "== Dongle in /proc/bus/input/devices (Vendor dezimal 4842!) =="
grep -B1 -A6 -i 'vendor=4842' /proc/bus/input/devices
echo
echo "== Pointer-Socket =="
luna-send -n 1 luna://com.webos.service.networkinput/getPointerInputSocket '{}'
echo
echo "== Naechste manuelle Schritte =="
echo "T2: pointer-test (aus dist/pointer-test.abi) nach /media/developer scp'en, chmod +x, starten;"
echo "    mit den BPR1S-Pfeiltasten einen Menuepunkt fokussieren; nach 5 s wird ENTER gesendet -> oeffnet?"
echo "T4: App oeffnen (z.B. YouTube), dann Home-Taste druecken (Daemon laufend) oder:"
echo "  luna-send -n 1 luna://com.webos.applicationManager/launch '{\"id\":\"com.webos.app.home\"}'"
echo "T3: in ein Untermenue navigieren, Zurueck-Taste der BPR1S druecken"
