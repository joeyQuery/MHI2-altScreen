#!/bin/ksh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# PHASE 3 (r1 build, tag audi-mu0678-r1) - source-only GEN2 preload for Audi MU0678. REVERSIBLE. NO DISPLAY
# TAKEOVER: nothing here writes to /dev/mlb/* or touches DisplayManager.
# Effect: dio_manager (CarPlay child) loads the GEN2 Stream-111 hook; H.264 is
# exposed only on 127.0.0.1:19820 for capture to a file.
#
# usage: ksh mu0678_phase3_install_r1.sh HELPER_SHA256SUM HOOK_SO PATCHED_JSON
# Refuses unless every input matches its exact expected hash.

SHA=$1; HOOK_SRC=$2; NEWJSON=$3
[ -x "$SHA" ] && [ -r "$HOOK_SRC" ] && [ -r "$NEWJSON" ] || { echo "usage: $0 sha256sum hook.so patched.json"; exit 64; }

EXP_AIRPLAY=a3ab0fe0ac20a712f7f240c1abd852a38faf78f2673880342d0eb24a6d8c29be
EXP_STOCKJSON=6e7968e0279a2548c50285923e5bbc66f8775ad490c2394e430318d570faeed9
EXP_HOOK=40fd679f5967a7a124821b1bc3cefc9372b09a934154f3d56bcad995b675bf20
OLD_HOOK=f3efa9f09972422307f1b9e37e323539036d935f9cc630a1ed7d8f223a2bbae2   # upstream GEN2 currently installed by phase 2
EXP_NEWJSON=${MU0678_EXPECTED_PATCHED_SHA256:-67e05b2faa15c82dc3c48937aeb6120bb0db4408084815be2f1aff096ce46a95}

TARGET=/mnt/system/etc/eso/production/smartphone_integrator.json
HOOK=/mnt/app/eso/lib/libmibr_carplay111.so
BACKDIR=/mnt/app/root/mibr-carplay111-backup
BACKUP=$BACKDIR/smartphone_integrator.json.stock

h(){ "$SHA" "$1" 2>/dev/null | awk '{print $1}'; }
die(){ echo "ABORT: $*"; mount -ur /mnt/app 2>/dev/null; mount -ur /mnt/system 2>/dev/null; exit 1; }

[ "$(h /mnt/app/eso/lib/libairplay.so)" = "$EXP_AIRPLAY" ] || die "libairplay hash mismatch - not the analysed build"
[ "$(h "$HOOK_SRC")" = "$EXP_HOOK" ]   || die "hook hash mismatch"
[ "$(h "$NEWJSON")" = "$EXP_NEWJSON" ] || die "patched json hash mismatch"
CUR=$(h "$TARGET")
[ -r "$BACKUP" ] || [ "$CUR" = "$EXP_STOCKJSON" ] || die "current config is not canonical stock and no backup exists"
[ -e "$HOOK" ] && [ "$(h "$HOOK")" != "$EXP_HOOK" ] && [ "$(h "$HOOK")" != "$OLD_HOOK" ] && die "unknown file already at $HOOK"

# 1. backup (internal storage, verified) + hook install
mount -uw /mnt/app || die "cannot remount /mnt/app rw"
mkdir -p "$BACKDIR" || die "cannot create $BACKDIR"
[ -e "$HOOK" ] && [ "$(h "$HOOK")" = "$OLD_HOOK" ] && [ ! -e "$BACKDIR/libmibr_carplay111.so.upstream-gen2" ] && cp "$HOOK" "$BACKDIR/libmibr_carplay111.so.upstream-gen2"
if [ ! -r "$BACKUP" ]; then
  cp "$TARGET" "$BACKUP" || die "backup copy failed"
  echo "$EXP_STOCKJSON  $BACKUP" > "$BACKUP.sha256"
fi
[ "$(h "$BACKUP")" = "$EXP_STOCKJSON" ] || die "backup hash mismatch"
cp "$HOOK_SRC" "$HOOK.new" && chmod 755 "$HOOK.new" && mv "$HOOK.new" "$HOOK" || die "hook install failed"
sync
[ "$(h "$HOOK")" = "$EXP_HOOK" ] || die "installed hook hash mismatch"
mount -ur /mnt/app

# 2. activate: replace the one config that makes dio_manager preload the hook
if [ "$CUR" != "$EXP_NEWJSON" ]; then
  mount -uw /mnt/system || die "cannot remount /mnt/system rw"
  cp "$NEWJSON" "$TARGET.new" && chmod 644 "$TARGET.new" && mv "$TARGET.new" "$TARGET" || die "config install failed"
  sync
  mount -ur /mnt/system
fi
[ "$(h "$TARGET")" = "$EXP_NEWJSON" ] || die "post-install config hash mismatch"

echo "INSTALLED (phase 2, source-only). REBOOT_REQUIRED=YES - do not hot-restart smartphone_integrator/dio_manager."
echo "Restore: ksh mu0678_phase2_restore.sh HELPER_SHA256SUM"
exit 0
