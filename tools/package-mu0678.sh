#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

# Build/package the MU0678 external cluster together with the exact Q reference
# live-CarPlay display helper. The helper is intentionally copied byte-for-byte
# from the supplied working install.zip; it is not rebuilt or modified here.

REF=${1:-}
OUT=${2:-package-mu0678}
CLUSTER=${3:-build/libmhi2qcarplaycluster.so}
DIO=${4:-}
if [ -z "$DIO" ] || [ ! -f "$DIO" ]; then
    echo "target MU0678 dio_manager missing: pass [dio-manager]" >&2
    exit 1
fi
if [ ! -f "$CLUSTER" ]; then
    echo "built cluster missing: $CLUSTER" >&2
    exit 1
fi
HELPER_SHA=0ad556112e427745da99e057affde5b18195db875398d2111cb131f790b7d65e

if [ -z "$REF" ] || [ ! -f "$REF" ]; then
    echo "usage: $0 /path/to/install.zip [output-dir] [cluster-so] [dio-manager]" >&2
    exit 2
fi

rm -rf "$OUT"
mkdir -p "$OUT/eso/bin/apps" "$OUT/eso/lib"
if readelf -d "$DIO" 2>/dev/null | grep -F "libmhi2qcarplaycluster.so" >/dev/null 2>&1; then
    cp "$DIO" "$OUT/eso/bin/apps/dio_manager"
else
    python3 "$ROOT/tools/patch-mu0678-dio.py" "$DIO" "$OUT/eso/bin/apps/dio_manager"
fi
chmod 755 "$OUT/eso/bin/apps/dio_manager"
cp "$CLUSTER" "$OUT/eso/lib/libmhi2qcarplaycluster.so"
chmod 755 "$OUT/eso/lib/libmhi2qcarplaycluster.so"

TMP=`mktemp -d`
trap 'rm -rf "$TMP"' EXIT
unzip -q "$REF" -d "$TMP"

HELPER="$TMP/Bootstrap/final/product/files/mhi2q-aa-cluster-display"
[ -f "$HELPER" ] || { echo "reference helper missing" >&2; exit 1; }

ACTUAL=`sha256sum "$HELPER" | awk '{print $1}'`
[ "$ACTUAL" = "$HELPER_SHA" ] || {
    echo "reference helper SHA mismatch: $ACTUAL" >&2
    exit 1
}



cp "$HELPER" "$OUT/eso/bin/apps/mhi2q-aa-cluster-display"
chmod 755 "$OUT/eso/bin/apps/mhi2q-aa-cluster-display"

echo "staged target MU0678 dio_manager: $OUT/eso/bin/apps/dio_manager"
CLUSTER_SHA=`sha256sum "$OUT/eso/lib/libmhi2qcarplaycluster.so" | awk '{print $1}'`
echo "staged MU0678 cluster: $OUT/eso/lib/libmhi2qcarplaycluster.so"
echo "cluster sha256: $CLUSTER_SHA"
echo "staged exact reference helper: $OUT/eso/bin/apps/mhi2q-aa-cluster-display"
echo "sha256: $HELPER_SHA"
