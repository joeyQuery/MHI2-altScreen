#!/bin/ksh
# SPDX-License-Identifier: GPL-3.0-or-later
# PHASE 2b - capture the Stream-111 tee to an MPEG-TS FILE in /tmp. Never writes to /dev/*.
# usage: ksh mu0678_phase2_capture.sh /path/to/direct-ts-remux [seconds]
BIN=$1; SEC=${2:-60}
[ -x "$BIN" ] || { echo "usage: $0 direct-ts-remux [seconds<=600]"; exit 64; }
OUTF=/tmp/mu0678-capture.ts
rm -f "$OUTF"
echo "waiting up to 120s for the H.264 tee on 127.0.0.1:19820 (start CarPlay navigation on the phone now)"
"$BIN" tcp://127.0.0.1:19820 "$OUTF" 30 "$SEC" 120 0x11
RC=$?
echo "direct-ts-remux rc=$RC"
ls -l "$OUTF" 2>&1
[ -r /tmp/mibr-direct-remux.status ] && cat /tmp/mibr-direct-remux.status
echo "copy off: scp root@<unit>:$OUTF ."
exit $RC
