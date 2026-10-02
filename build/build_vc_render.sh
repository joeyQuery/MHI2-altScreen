#!/bin/bash
set -eu
QNX_RUN="${QNX_RUN:-$HOME/Documents/GitHub/MHI2-CarPlay-Alternate-Screen/.qnx-toolchain/host-scripts/qnx-run.sh}"
[ -x "$QNX_RUN" ] || { echo "qnx-run.sh not found/executable: $QNX_RUN (set QNX_RUN=)"; exit 1; }
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../src/vc_render"
OUT="$HERE/out"
mkdir -p "$OUT"
cd "$SRC"
"$QNX_RUN" qcc -Vgcc_ntoarmv7le -mfloat-abi=softfp -O2 -g -std=gnu99 -Wall -Wextra -o vc_render vc_render.c -lsocket
mv -f vc_render "$OUT/vc_render"
shasum -a 256 "$OUT/vc_render"
echo "Built $OUT/vc_render"
