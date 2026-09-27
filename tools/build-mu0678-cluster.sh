#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../src/mu0678" && pwd)
OUT=${1:-"$ROOT/../../build/libmhi2qcarplaycluster.so"}
mkdir -p "$(dirname "$OUT")"

# The working reference is QNX ARM, not Linux ARM EABI. Never silently emit a
# Linux ELF as a MU0678 payload. Prefer a QNX compiler/linker when installed.
if command -v qcc >/dev/null 2>&1; then
    CC=qcc
    "$CC" -Vgcc_ntoarmv7le -shared -fPIC -march=armv7-a -marm \
        -fno-builtin -fno-stack-protector -I"$ROOT" \
        "$ROOT/mu0678_cluster.c" "$ROOT/mu0678_interpose.c" \
        -Wl,-soname,libmhi2qcarplaycluster.so \
        -o "$OUT"
elif command -v ntoarmv7-gcc >/dev/null 2>&1; then
    CC=ntoarmv7-gcc
    "$CC" -shared -fPIC -march=armv7-a -marm \
        -fno-builtin -fno-stack-protector -I"$ROOT" \
        "$ROOT/mu0678_cluster.c" "$ROOT/mu0678_interpose.c" \
        -Wl,-soname,libmhi2qcarplaycluster.so \
        -o "$OUT"
else
    echo "No QNX ARM toolchain found (qcc or ntoarmv7-gcc)." >&2
    echo "Refusing to produce a Linux/EABI binary that cannot be treated as a MU0678 payload." >&2
    exit 2
fi

file "$OUT"
readelf -h "$OUT" | grep -E 'Class|Data|Type|Machine'
readelf -d "$OUT" | grep -E 'NEEDED|SONAME'
