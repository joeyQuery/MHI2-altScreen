#!/bin/sh
set -eu

# Build/package the MU0678 external cluster together with the exact Q reference
# live-CarPlay display helper. The helper is intentionally copied byte-for-byte
# from the supplied working install.zip; it is not rebuilt or modified here.

REF=${1:-}
if [ ! -f "$CLUSTER" ]; then
    echo "built cluster missing: $CLUSTER" >&2
    exit 1
fi
OUT=${2:-package-mu0678}
CLUSTER=${3:-build/libmhi2qcarplaycluster.so}
DIO_LOADER_SHA=4dbbb057fe5ccbd95fbea71b16810cbdd417ca60a7aa6113e7918ac97e252e94
HELPER_SHA=0ad556112e427745da99e057affde5b18195db875398d2111cb131f790b7d65e

if [ -z "$REF" ] || [ ! -f "$REF" ]; then
    echo "usage: $0 /path/to/install.zip [output-dir]" >&2
    exit 2
fi

rm -rf "$OUT"
mkdir -p "$OUT/eso/bin/apps" "$OUT/eso/lib"

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


DIO_LOADER="$TMP/Bootstrap/final/product/files/dio_manager.mhi2q_carplay_loader"
[ -f "$DIO_LOADER" ] || { echo "reference dio loader missing" >&2; exit 1; }
ACTUAL_DIO=`sha256sum "$DIO_LOADER" | awk '{print $1}'`
[ "$ACTUAL_DIO" = "$DIO_LOADER_SHA" ] || {
    echo "reference dio loader SHA mismatch: $ACTUAL_DIO" >&2
    exit 1
}

cp "$CLUSTER" "$OUT/eso/lib/libmhi2qcarplaycluster.so"
chmod 755 "$OUT/eso/lib/libmhi2qcarplaycluster.so"
cp "$DIO_LOADER" "$OUT/eso/bin/apps/dio_manager"
chmod 755 "$OUT/eso/bin/apps/dio_manager"
cp "$HELPER" "$OUT/eso/bin/apps/mhi2q-aa-cluster-display"
chmod 755 "$OUT/eso/bin/apps/mhi2q-aa-cluster-display"

echo "staged reference DIO loader: $OUT/eso/bin/apps/dio_manager"
echo "staged MU0678 cluster: $OUT/eso/lib/libmhi2qcarplaycluster.so"
echo "staged exact reference helper: $OUT/eso/bin/apps/mhi2q-aa-cluster-display"
echo "sha256: $HELPER_SHA"
