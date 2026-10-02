#!/bin/sh
# Undo hud_install.sh.  Run on the unit: sh /tmp/hud/hud_rollback.sh   then restart.
set -u
JARS=/mnt/app/eso/hmi/lsd/jars
LIB=/mnt/app/eso/hmi/lib
SBIN=/mnt/app/armle/usr/sbin
mount -uw /mnt/app 2>/dev/null
# jar first: it is the HMI-side part
rm -f "$JARS/rgd_hook.jar" "$JARS/rgd_hook.jar.new"
if [ -f "$SBIN/mm-ipod.orig" ]; then
    cp -p "$SBIN/mm-ipod.orig" "$SBIN/mm-ipod.back" && mv "$SBIN/mm-ipod.back" "$SBIN/mm-ipod" && echo "restored mm-ipod from .orig"
else
    echo "!! $SBIN/mm-ipod.orig missing - mm-ipod NOT restored"
fi
rm -f "$LIB/librgd_hook.so" "$LIB/librgd_hook.so.new" "$LIB/rgd_blank.png"
rm -f "$SBIN/rgd_real/mm-ipod"
rm -r "$SBIN/rgd_real" 2>/dev/null
sync; sleep 1; sync
ls -l "$SBIN/mm-ipod" "$JARS" 2>&1 | head -20
echo "=== rolled back. Restart the car. ==="
