#!/bin/bash
# Build the Audi-adapted GEN2 hook (libaltscreen111.so) on the Mac with the local QNX toolchain.
# Usage: from anywhere:  QNX_RUN=~/Documents/GitHub/MHI2-CarPlay-Alternate-Screen/.qnx-toolchain/host-scripts/qnx-run.sh \
#        bash audi-mu0678/build/build_gen2_audi.sh
set -eu
QNX_RUN="${QNX_RUN:-$HOME/Documents/GitHub/MHI2-CarPlay-Alternate-Screen/.qnx-toolchain/host-scripts/qnx-run.sh}"
[ -x "$QNX_RUN" ] || { echo "qnx-run.sh not found/executable: $QNX_RUN (set QNX_RUN=)"; exit 1; }
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../src/gen2-audi"
OUT="$HERE/out"
mkdir -p "$OUT"
cd "$SRC"
"$QNX_RUN" qcc -Vgcc_ntoarmv7le -shared -fPIC -mfloat-abi=softfp -O2 -g -std=gnu99 -Wall -Wextra \
  -I include -o libaltscreen111.so \
  libaltscreen111_gen2.c src/alt111_profile.c src/alt111_control.c src/alt111_video.c src/alt111_resync.c \
  -Wl,-soname,libaltscreen111.so -lsocket
mv -f libaltscreen111.so "$OUT/libaltscreen111.so"
shasum -a 256 "$OUT/libaltscreen111.so"
echo "Built $OUT/libaltscreen111.so (tag audi-mu0678-r5-nativeb-p1)"
