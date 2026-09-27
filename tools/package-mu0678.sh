#!/bin/sh
set -eu

# Build/package the MU0678 external cluster together with the exact Q reference
# live-CarPlay display helper. The helper is intentionally copied byte-for-byte
# from the supplied working install.zip; it is not rebuilt or modified here.

REF=${1:-}
OUT=${2:-package-mu0678}
HELPER_SHA=0ad556112e427745da99e057affde5b18195db875398d2111cb131f790b7d65e

if [ -z "$REF" ] || [ ! -f "$REF" ]; then
    echo "usage: $0 /path/to/install.zip [output-dir]" >&2
    exit 2
fi

rm -rf "$OUT"
mkdir -p "$OUT/eso/bin/apps"

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

echo "staged exact reference helper: $OUT/eso/bin/apps/mhi2q-aa-cluster-display"
echo "sha256: $HELPER_SHA"
