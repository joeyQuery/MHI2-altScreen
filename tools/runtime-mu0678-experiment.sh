#!/bin/sh
set -eu
# MU0678 AltScreen runtime experiment controller.
# Target shell is restricted QNX: do not assume date, awk, sha256sum, file,
# od, hexdump, or strings exist.
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
show_file() { f=$1; if [ -f "$f" ]; then ls -l "$f"; else echo "MISSING  $f"; fi; }
backup_one() {
    src=$1; name=$2
    if [ -e "$src" ]; then cp -p "$src" "$BACKUP/$name"; echo "backup $src -> $BACKUP/$name"
    else echo "absent $src" > "$BACKUP/$name.absent"; fi
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
        echo "pid=$$"
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
    show_file "$DIO_DST"; show_file "$CLUSTER_DST"; show_file "$HELPER_DST"
    echo
    echo "Reboot the MHI2 before the experiment. Do not hot-restart dio_manager."
}
status() {
    echo "=== payloads ==="
    show_file "$DIO_DST"; show_file "$CLUSTER_DST"; show_file "$HELPER_DST"
    echo; echo "=== processes ==="
    pidin ar 2>/dev/null | grep -E 'dio_manager|mhi2q-aa-cluster-display' || true
    echo; echo "=== sockets/listeners ==="
    netstat -an 2>/dev/null | grep -E '15557|LISTEN' || true
}
collect() {
    need_root
    mkdir -p "$LOGROOT"
    out="$LOGROOT/collect-$$"; n=0
    while [ -e "$out" ]; do n=`expr "$n" + 1`; out="$LOGROOT/collect-$$-$n"; done
    mkdir -p "$out"; echo "$out"
    pidin ar > "$out/pidin-ar.txt" 2>&1 || true
    pidin threads > "$out/pidin-threads.txt" 2>&1 || true
    pidin mem > "$out/pidin-mem.txt" 2>&1 || true
    netstat -an > "$out/netstat.txt" 2>&1 || true
    sloginfo > "$out/sloginfo.txt" 2>&1 || true
    { show_file "$DIO_DST"; show_file "$CLUSTER_DST"; show_file "$HELPER_DST"; } > "$out/payload-files.txt"
    for f in /mnt/app/root/dio_manager.live /mnt/app/root/dio_manager.json /mnt/system/etc/eso/production/dio_manager.json; do
        if [ -f "$f" ]; then cp -p "$f" "$out/`basename "$f"`"; fi
    done
    echo "logs written to $out"
}
rollback() {
    need_root
    [ -d "$BACKUP" ] || die "no experiment backup at $BACKUP"
    restore_one() {
        dst=$1; name=$2
        if [ -f "$BACKUP/$name" ]; then cp -p "$BACKUP/$name" "$dst"
        elif [ -f "$BACKUP/$name.absent" ]; then rm -f "$dst"; fi
    }
    restore_one "$DIO_DST" dio_manager
    restore_one "$CLUSTER_DST" libmhi2qcarplaycluster.so
    restore_one "$HELPER_DST" mhi2q-aa-cluster-display
    echo "rollback complete. Reboot the MHI2 to return to the pre-experiment process image."
}
case "${1:-}" in
    install) install_payload ;;
    status) status ;;
    collect) collect ;;
    rollback) rollback ;;
    *) echo "usage: $0 {install|status|collect|rollback}"; exit 2 ;;
esac
