#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
# Build/package the MU0678 external cluster with the exact Q reference helper.
REF=${1:-}
OUT=${2:-package-mu0678}
CLUSTER=${3:-build/libmhi2qcarplaycluster.so}
DIO=${4:-}
[ -n "$DIO" ] && [ -f "$DIO" ] || { echo "target MU0678 dio_manager missing: pass [dio-manager]" >&2; exit 1; }
[ -f "$CLUSTER" ] || { echo "built cluster missing: $CLUSTER" >&2; exit 1; }
[ -n "$REF" ] && [ -f "$REF" ] || { echo "usage: $0 /path/to/install.zip [output-dir] [cluster-so] [dio-manager]" >&2; exit 2; }
HELPER_SHA=0ad556112e427745da99e057affde5b18195db875398d2111cb131f790b7d65e
rm -rf "$OUT"
mkdir -p "$OUT/eso/bin/apps" "$OUT/eso/lib"
python3 "$ROOT/tools/patch-mu0678-dio.py" "$DIO" "$OUT/eso/bin/apps/dio_manager"
chmod 755 "$OUT/eso/bin/apps/dio_manager"
cp "$CLUSTER" "$OUT/eso/lib/libmhi2qcarplaycluster.so"
chmod 755 "$OUT/eso/lib/libmhi2qcarplaycluster.so"
TMP=`mktemp -d`; trap 'rm -rf "$TMP"' EXIT
unzip -q "$REF" -d "$TMP"
HELPER="$TMP/Bootstrap/final/product/files/mhi2q-aa-cluster-display"
[ -f "$HELPER" ] || { echo "reference helper missing" >&2; exit 1; }
ACTUAL=`sha256sum "$HELPER" | awk '{print $1}'`
[ "$ACTUAL" = "$HELPER_SHA" ] || { echo "reference helper SHA mismatch: $ACTUAL" >&2; exit 1; }
cp "$HELPER" "$OUT/eso/bin/apps/mhi2q-aa-cluster-display"
chmod 755 "$OUT/eso/bin/apps/mhi2q-aa-cluster-display"
cp "$ROOT/tools/runtime-mu0678-experiment.sh" "$OUT/runtime-mu0678-experiment.sh"
chmod 755 "$OUT/runtime-mu0678-experiment.sh"

# qnx-run mounts its caller's working directory as /src. Validate copies placed
# under ROOT so absolute output paths outside the repo cannot break validation.
QNX_RUN=${QNX_RUN:-"$ROOT/../MHI2-CarPlay-Alternate-Screen/.qnx-toolchain/host-scripts/qnx-run.sh"}
[ -x "$QNX_RUN" ] || { echo "QNX validation wrapper missing: $QNX_RUN" >&2; exit 1; }
QNX_TMP="$ROOT/.qnx-validate-mu0678-$$"
rm -rf "$QNX_TMP"
mkdir -p "$QNX_TMP"
trap 'rm -rf "$TMP" "$QNX_TMP"' EXIT
cp "$OUT/eso/bin/apps/dio_manager" "$QNX_TMP/dio_manager"
cp "$OUT/eso/lib/libmhi2qcarplaycluster.so" "$QNX_TMP/libmhi2qcarplaycluster.so"
(
    cd "$ROOT"
    "$QNX_RUN" sh -c 'readelf -d /src/"$1" | grep -F "libmhi2qcarplaycluster.so" >/dev/null' sh ".qnx-validate-mu0678-$$/dio_manager"
    "$QNX_RUN" sh -c 'readelf -d /src/"$1" | grep -E "SONAME.*libmhi2qcarplaycluster.so" >/dev/null' sh ".qnx-validate-mu0678-$$/libmhi2qcarplaycluster.so"
    "$QNX_RUN" sh -c 'readelf -d /src/"$1" | grep -F "libsocket.so.3" >/dev/null' sh ".qnx-validate-mu0678-$$/libmhi2qcarplaycluster.so"
)
rm -rf "$QNX_TMP"
trap - EXIT

echo "staged target MU0678 dio_manager: $OUT/eso/bin/apps/dio_manager"
echo "staged MU0678 cluster: $OUT/eso/lib/libmhi2qcarplaycluster.so"
echo "staged exact reference helper: $OUT/eso/bin/apps/mhi2q-aa-cluster-display"
echo "helper sha256: $HELPER_SHA"
