#!/bin/sh
# TV-side tests for the BPR1S key mapper. Run on the TV as root via SSH:
#   ssh root@<tv> -p 9922 'sh -s' < tools/tv-test.sh
echo "== Kernel / userland =="
uname -r
uname -m
getconf LONG_BIT
echo
echo "== T1: ABI of /bin/busybox (e_flags, bit 0x400 = hard float) =="
od -An -tx4 -j36 -N4 /bin/busybox
ls -l /lib/ld-linux* 2>/dev/null
echo
echo "== luna-send path =="
which luna-send
echo
echo "== Dongle in /proc/bus/input/devices (vendor decimal 4842!) =="
grep -B1 -A6 -i 'vendor=4842' /proc/bus/input/devices
echo
echo "== Next manual steps =="
echo "T2: scp dist/bpr1s-keymapper.abi to /media/developer, chmod +x, run with --verbose;"
echo "    press OK/Home/Back/Menu on the BPR1S -> check /tmp/km.log"
echo "T3: open an app (e.g. YouTube), press Home (daemon running) or:"
echo "  luna-send -n 1 luna://com.webos.applicationManager/launch '{\"id\":\"com.webos.app.home\"}'"
echo "T4: navigate into a submenu, press Back on the BPR1S"
