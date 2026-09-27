#!/bin/sh
set -eu

# MU0678 AltScreen runtime experiment controller.
# Run on the target MHI2 unit as root.
#
# The experiment is deliberately transactional:
#   install  -> backs up every replaced payload, then stages the experiment
#   status   -> reports payload hashes/process state
#   collect  -> captures diagnostic state to /tmp/mu0678-alt111-<timestamp>
#   rollback -> restores the exact pre-experiment payloads
#
# A reboot is the preferred activation/deactivation boundary. Do not hot-restart
# dio_manager unless you already know the supervisor configuration for this unit.

ROOT=/eso
LIB=$ROOT/lib
APP=$ROOT/bin/apps
BACKUP=/tmp/mu0678-alt111-backup
LOGROOT=/tmp/mu0678-alt111-logs

DIO_SRC=${DIO_SRC:-./dio_manager}
CLUSTER_SRC=${CLUSTER_SRC:-./libmhi2qcarplaycluster.so}
HELPER_SRC=${HELPER_SRC:-./mhi2q-aa-cluster-display}

DIO_DST=$APP/dio_manager
CLUSTER_DST=$LIB/libmhi2qcarplaycluster.so
HELPER_DST=$APP/mhi2q-aa-cluster-display

die() { echo "ERROR: $*" >&2; exit 1; }
need_root() { [ "`id -u`" = 0 ] || die "run as root"; }
sha() { sha256sum "$1" 2>/dev/null | awk '{print $1}'; }

backup_one() {
    src=$1
    name=$2
    if [ -e "$src" ]; then
        cp -p "$src" "$BACKUP/$name"
        echo "backup $src -> $BACKUP/$name"
    else
        echo "absent $src" > "$BACKUP/$name.absent"
    fi
}

install_payload() {
    need_root
    [ -f "$DIO_SRC" ] || die "missing $DIO_SRC"
    [ -f "$CLUSTER_SRC" ] || die "missing $CLUSTER_SRC"
    [ -f "$HELPER_SRC" ] || die "missing $HELPER_SRC"

    mkdir -p "$BACKUP"
    [ ! -e "$BACKUP/manifest" ] || die "backup already exists at $BACKUP; rollback it first"

    {
        echo "MU0678 AltScreen runtime experiment"
        echo "date=`date`"
        echo "dio_dst=$DIO_DST"
        echo "cluster_dst=$CLUSTER_DST"
        echo "helper_dst=$HELPER_DST"
    } > "$BACKUP/manifest"

    backup_one "$DIO_DST" dio_manager
    backup_one "$CLUSTER_DST" libmhi2qcarplaycluster.so
    backup_one "$HELPER_DST" mhi2q-aa-cluster-display

    cp -p "$DIO_SRC" "$DIO_DST"
    cp -p "$CLUSTER_SRC" "$CLUSTER_DST"
    cp -p "$HELPER_SRC" "$HELPER_DST"
    chmod 755 "$DIO_DST" "$CLUSTER_DST" "$HELPER_DST"

    echo "installed:"
    echo "  dio_manager:                  $DIO_DST  `sha "$DIO_DST"`"
    echo "  libmhi2qcarplaycluster.so:    $CLUSTER_DST  `sha "$CLUSTER_DST"`"
    echo "  mhi2q-aa-cluster-display:     $HELPER_DST  `sha "$HELPER_DST"`"
    echo
    echo "Reboot the MHI2 before the experiment. Do not hot-restart dio_manager."
}

status() {
    echo "=== payloads ==="
    for f in "$DIO_DST" "$CLUSTER_DST" "$HELPER_DST"; do
        if [ -f "$f" ]; then
            echo "`sha "$f"`  $f"
        else
            echo "MISSING  $f"
        fi
    done
    echo
    echo "=== processes ==="
    pidin ar 2>/dev/null | grep -E 'dio_manager|mhi2q-aa-cluster-display' || true
    echo
    echo "=== sockets/listeners ==="
    netstat -an 2>/dev/null | grep -E '15557|LISTEN' || true
}

collect() {
    need_root
    stamp=`date +%Y%m%d-%H%M%S`
    out="$LOGROOT/$stamp"
    mkdir -p "$out"
    echo "$out"

    date > "$out/date.txt"
    pidin ar > "$out/pidin-ar.txt" 2>&1 || true
    pidin threads > "$out/pidin-threads.txt" 2>&1 || true
    pidin mem > "$out/pidin-mem.txt" 2>&1 || true
    netstat -an > "$out/netstat.txt" 2>&1 || true
    sloginfo > "$out/sloginfo.txt" 2>&1 || true

    for f in "$DIO_DST" "$CLUSTER_DST" "$HELPER_DST"; do
        if [ -f "$f" ]; then
            sha "$f" >> "$out/payload-sha256.txt"
        fi
    done

    # Preserve the DIO configuration inputs without modifying them.
    for f in /mnt/app/root/dio_manager.live /mnt/app/root/dio_manager.json \
             /mnt/system/etc/eso/production/dio_manager.json; do
        if [ -f "$f" ]; then
            cp -p "$f" "$out/`basename "$f"`"
        fi
    done

    echo "logs written to $out"
}

rollback() {
    need_root
    [ -d "$BACKUP" ] || die "no experiment backup at $BACKUP"

    restore_one() {
        dst=$1
        name=$2
        if [ -f "$BACKUP/$name" ]; then
            cp -p "$BACKUP/$name" "$dst"
        elif [ -f "$BACKUP/$name.absent" ]; then
            rm -f "$dst"
        fi
    }

    restore_one "$DIO_DST" dio_manager
    restore_one "$CLUSTER_DST" libmhi2qcarplaycluster.so
    restore_one "$HELPER_DST" mhi2q-aa-cluster-display

    echo "rollback complete. Reboot the MHI2 to return to the pre-experiment process image."
}

case "${1:-}" in
    install)  install_payload ;;
    status)   status ;;
    collect)  collect ;;
    rollback) rollback ;;
    *)
        echo "usage: $0 {install|status|collect|rollback}"
        exit 2
        ;;
esac
