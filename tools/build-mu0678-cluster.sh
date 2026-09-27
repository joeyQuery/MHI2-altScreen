#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../src/mu0678" && pwd)
OUT=${1:-"$ROOT/../../build/libmhi2mu0678altscreen.so"}
mkdir -p "$(dirname "$OUT")"
clang --target=armv7-none-eabi -marm -fPIC -fno-builtin -fno-stack-protector -ffreestanding -I"$ROOT" -c "$ROOT/mu0678_cluster.c" -o "$ROOT/../../build/mu0678_cluster.o"
clang --target=armv7-none-eabi -marm -fPIC -fno-builtin -fno-stack-protector -ffreestanding -I"$ROOT" -c "$ROOT/mu0678_interpose.c" -o "$ROOT/../../build/mu0678_interpose.o"
ld.lld -m armelf_linux_eabi -shared --unresolved-symbols=ignore-all "$ROOT/../../build/mu0678_cluster.o" "$ROOT/../../build/mu0678_interpose.o" -o "$OUT"
file "$OUT"
readelf -h "$OUT" | grep -E 'Class|Data|Type|Machine'
