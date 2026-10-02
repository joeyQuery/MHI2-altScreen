#!/bin/ksh
# SPDX-License-Identifier: GPL-3.0-or-later
# Restores stock smartphone_integrator.json from the verified backup and removes the hook.
# usage: ksh mu0678_phase2_restore.sh HELPER_SHA256SUM
SHA=$1
[ -x "$SHA" ] || { echo "usage: $0 sha256sum-helper"; exit 64; }
EXP_STOCKJSON=6e7968e0279a2548c50285923e5bbc66f8775ad490c2394e430318d570faeed9
TARGET=/mnt/system/etc/eso/production/smartphone_integrator.json
HOOK=/mnt/app/eso/lib/libmibr_carplay111.so
BACKUP=/mnt/app/root/mibr-carplay111-backup/smartphone_integrator.json.stock
h(){ "$SHA" "$1" 2>/dev/null | awk '{print $1}'; }
FAIL=0
if [ -r "$BACKUP" ] && [ "$(h "$BACKUP")" = "$EXP_STOCKJSON" ]; then
  mount -uw /mnt/system && cp "$BACKUP" "$TARGET.restore" && chmod 644 "$TARGET.restore" && mv "$TARGET.restore" "$TARGET" && sync
  mount -ur /mnt/system
else
  [ "$(h "$TARGET")" = "$EXP_STOCKJSON" ] || { echo "ERROR: no valid backup and target is not stock"; FAIL=1; }
fi
[ "$(h "$TARGET")" = "$EXP_STOCKJSON" ] && echo "config: STOCK" || { echo "config: NOT STOCK"; FAIL=1; }
# only remove the hook once the stock config is confirmed
if [ "$FAIL" -eq 0 ] && [ -e "$HOOK" ]; then
  mount -uw /mnt/app && rm -f "$HOOK" && sync
  mount -ur /mnt/app
fi
[ "$FAIL" -eq 0 ] && echo "RESTORED. Reboot to return to stock behaviour." || echo "RESTORE INCOMPLETE - do not reboot blind; investigate."
exit $FAIL
