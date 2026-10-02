#!/bin/ksh
# SPDX-License-Identifier: GPL-3.0-or-later
# Swap ONLY the preload hook file (config untouched). Reversible: previous hook is
# kept in the backup dir. usage: ksh mu0678_hook_swap.sh SHA256SUM_HELPER NEW_HOOK_SO EXPECTED_SHA256
SHA=$1; SRC=$2; EXP=$3
[ -x "$SHA" ] && [ -r "$SRC" ] && [ -n "$EXP" ] || { echo "usage: $0 sha256sum new.so expected_sha256"; exit 64; }
HOOK=/mnt/app/eso/lib/libmibr_carplay111.so
BACK=/mnt/app/root/mibr-carplay111-backup
h(){ "$SHA" "$1" 2>/dev/null | awk '{print $1}'; }
die(){ echo "ABORT: $*"; mount -ur /mnt/app 2>/dev/null; exit 1; }
[ "$(h /mnt/app/eso/lib/libairplay.so)" = "a3ab0fe0ac20a712f7f240c1abd852a38faf78f2673880342d0eb24a6d8c29be" ] || die "libairplay not the analysed build"
[ "$(h "$SRC")" = "$EXP" ] || die "new hook hash != expected"
[ -e "$HOOK" ] || die "no existing hook at $HOOK (run phase3 install first)"
mount -uw /mnt/app || die "remount rw failed"
mkdir -p "$BACK" || die "backup dir"
[ -e "$BACK/libmibr_carplay111.so.prev-$(h "$HOOK")" ] || cp "$HOOK" "$BACK/libmibr_carplay111.so.prev-$(h "$HOOK")" || die "backup failed"
cp "$SRC" "$HOOK.new" && chmod 755 "$HOOK.new" && mv "$HOOK.new" "$HOOK" || die "copy failed"
sync
[ "$(h "$HOOK")" = "$EXP" ] || die "installed hash mismatch"
mount -ur /mnt/app
echo "HOOK SWAPPED to $EXP. Re-plug the phone (dio_manager reloads it); reboot if the log does not say the new build tag."
