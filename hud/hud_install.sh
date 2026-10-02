#!/bin/sh
# HUD-only CarPlay route guidance (MU0678).  Run ON THE UNIT:
#   sh /tmp/hud/hud_install.sh
# Installs the route-guidance native hook (into mm-ipod), the HMI jar (patched:
# no cluster context switching, so the altscreen is not touched) and the blank
# overlay bitmap.  No renderer.  Rollback: sh /tmp/hud/hud_rollback.sh
# POSIX sh only; the unit has no sed/awk/printf/dirname.
set -u
SRC=${0%/*}
case $0 in */*) ;; *) SRC=. ;; esac
BIN=$SRC/bin
JARS=/mnt/app/eso/hmi/lsd/jars
LIB=/mnt/app/eso/hmi/lib
SBIN=/mnt/app/armle/usr/sbin
LOG=/tmp/hud_install.log
say() { echo "$@"; echo "$@" >> "$LOG" 2>/dev/null; }
die() { say "!! $*"; say "!! ABORTED"; exit 1; }
: > "$LOG" 2>/dev/null

for f in rgd_hook.jar librgd_hook.so rgd_blank.png; do
    [ -f "$BIN/$f" ] || die "missing $BIN/$f"
done
[ -d /mnt/app/eso ] || die "/mnt/app/eso not found"
[ -f "$SBIN/mm-ipod" ] || die "$SBIN/mm-ipod not found"
[ -d "$JARS" ] || die "$JARS not found"

DRV=""
for p in /mnt/app/armle/lib/dll/ipod-drvr-iap2.so /lib/dll/ipod-drvr-iap2.so /proc/boot/ipod-drvr-iap2.so; do
    [ -f "$p" ] && DRV=$p && break
done
if [ -z "$DRV" ]; then
    say "!! WARNING: ipod-drvr-iap2.so not found in the usual places - the hook may not engage."
else
    say "iAP2 driver: $DRV"
fi

mount -uw /mnt/app 2>/dev/null
touch /mnt/app/.rwtest 2>/dev/null || die "/mnt/app is not writable"
rm -f /mnt/app/.rwtest

[ -d "$LIB" ] || mkdir -p "$LIB" || die "cannot create $LIB"

# mm-ipod: keep the real binary under rgd_real/ and an .orig backup.
if [ ! -f "$SBIN/rgd_real/mm-ipod" ]; then
    mkdir -p "$SBIN/rgd_real" || die "cannot create $SBIN/rgd_real"
    cp -p "$SBIN/mm-ipod" "$SBIN/rgd_real/mm-ipod" || die "cannot copy mm-ipod aside"
    cp -p "$SBIN/mm-ipod" "$SBIN/mm-ipod.orig" || die "cannot back up mm-ipod"
    say "saved original mm-ipod: $SBIN/mm-ipod.orig"
else
    say "shim already installed - refreshing"
fi

# files (rename into place so a running process keeps its inode)
cp "$BIN/librgd_hook.so" "$LIB/librgd_hook.so.new" || die "copy librgd_hook.so failed"
chmod 755 "$LIB/librgd_hook.so.new"
mv "$LIB/librgd_hook.so.new" "$LIB/librgd_hook.so" || die "install librgd_hook.so failed"
cp "$BIN/rgd_blank.png" "$LIB/rgd_blank.png" || die "copy rgd_blank.png failed"
chmod 644 "$LIB/rgd_blank.png"
say "ok: $LIB/librgd_hook.so  $LIB/rgd_blank.png"

# the shim (no renderer supervisor)
cat > "$SBIN/mm-ipod.new" <<'SHIM'
#!/bin/sh
# HUD route-guidance shim. argv[0] stays "mm-ipod" (the hook gates on it).
# Off switch: touch /mnt/app/rgd_disable, then replug the phone.
[ -f /mnt/app/rgd_disable ] || LD_PRELOAD=/mnt/app/eso/hmi/lib/librgd_hook.so
export LD_PRELOAD
exec /mnt/app/armle/usr/sbin/rgd_real/mm-ipod "$@"
SHIM
chmod 755 "$SBIN/mm-ipod.new"
mv "$SBIN/mm-ipod.new" "$SBIN/mm-ipod" || die "cannot install the shim"
say "ok: $SBIN/mm-ipod (shim)"

# the HMI jar goes in LAST: it is the only part that can stop the HMI starting
cp "$BIN/rgd_hook.jar" "$JARS/rgd_hook.jar.new" || die "copy rgd_hook.jar failed"
chmod 755 "$JARS/rgd_hook.jar.new"
mv "$JARS/rgd_hook.jar.new" "$JARS/rgd_hook.jar" || die "install rgd_hook.jar failed"
say "ok: $JARS/rgd_hook.jar"

sync; sleep 2; sync
say ""
say "=== DONE. Restart the car (HMI) to load the jar. ==="
say "After it: plug the phone in, start a route, then:"
say "  cat /tmp/carplay_hook.log | tail -40"
say "  cat /mnt/app/rgd_hook.log | tail -40"
say "Rollback: sh $SRC/hud_rollback.sh   (then restart)"
say "Log of this install: $LOG"
