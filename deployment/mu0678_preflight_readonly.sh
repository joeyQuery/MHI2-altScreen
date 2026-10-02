#!/bin/ksh
# SPDX-License-Identifier: GPL-3.0-or-later
#
# PHASE 1 - READ-ONLY preflight for Audi MHI2 MU0678 (MHI2_ER_AUG22_P3241 profile).
# Writes NOTHING except a report directory under /tmp (RAM). No remount, no
# process is started, stopped or signalled.
#
# usage: ksh mu0678_preflight_readonly.sh /path/to/sha256sum-helper
#   helper = artifacts/mu1440/sha256sum-compat/build-confirmed-current/sha256sum
# QNX rules honoured: no printf/sed/tr, no grep alternation, one awk per need.

SHA=$1
[ -x "$SHA" ] || { echo "usage: $0 /path/to/executable/sha256sum-helper"; exit 64; }
OUT=/tmp/mu0678-preflight
mkdir -p "$OUT" || exit 65
R=$OUT/report.txt
: > "$R"

say(){ echo "$*" >> "$R"; echo "$*"; }
hl(){ # hash line
  if [ -r "$1" ]; then
    H=$("$SHA" "$1" 2>/dev/null | awk '{print $1}')
    Z=$(wc -c < "$1" 2>/dev/null)
    say "sha256 ${H:-FAILED} size=${Z:-?} $1"
  else
    say "missing $1"
  fi
}
sect(){ say ""; say "=== $* ==="; }

sect "expected (from static analysis of dump MU0678-MHI2_ER_AUG22_P3241)"
say "libairplay.so            a3ab0fe0ac20a712f7f240c1abd852a38faf78f2673880342d0eb24a6d8c29be"
say "smartphone_integrator.json 6e7968e0279a2548c50285923e5bbc66f8775ad490c2394e430318d570faeed9"
say "displaymanager.json      c2263eb38ff03dbd23d1c95ddf47e04762df97e40da1825f8677801cec24d644"
say "dio_manager.json         99a11a14efa4a02bc5811b5d29d0b336695089c06774ca4568bb7844f4688528"
say "dio_manager (bin)        0ca317e0549f... (prefix only; full value in evidence)"

sect "identity"
uname -a >> "$R" 2>&1
for F in /mnt/app/eso/hmi/lsd/version.txt /mnt/system/etc/project.txt /etc/project.txt /mnt/app/version_info.txt; do
  [ -r "$F" ] && { say "--- $F"; cat "$F" >> "$R" 2>&1; }
done

sect "hashes (app image, system, ifs)"
hl /mnt/app/eso/lib/libairplay.so
hl /mnt/app/eso/bin/apps/dio_manager
hl /mnt/app/eso/bin/apps/smartphone_integrator
hl /mnt/app/eso/bin/apps/videoovermost
hl /mnt/app/eso/bin/apps/dmdt
hl /mnt/app/eso/lib/factories/libdsicarplayproxy.so
hl /mnt/app/armle/lib/libnvss_video.so
hl /mnt/app/eso/hmi/lsd/lsd.sh
hl /ifs/lsd.jxe
for D in /mnt/system/etc/eso/production /etc/eso/production; do
  for J in smartphone_integrator.json displaymanager.json dio_manager.json videoovermost.json; do hl "$D/$J"; done
done
for P in /mnt/app/eso/bin/apps/displaymanager /ifs/displaymanager /usr/bin/displaymanager /proc/boot/displaymanager; do hl "$P"; done

sect "mounts / writability / space (read-only queries)"
mount 2>&1 | awk '/mnt\/app|mnt\/system|ifs|mnt\/ota|sda|sdb|mmc/ {print}' >> "$R"
df -k /mnt/app /mnt/system /tmp /mnt/ota 2>&1 >> "$R"
ls -ld /mnt/app/root /mnt/app/eso/lib /mnt/system/etc/eso/production 2>&1 >> "$R"

sect "processes of interest (full command lines)"
pidin ar 2>/dev/null | awk '
  index($0,"displaymanager") || index($0,"dio_manager") || index($0,"smartphone_integrator") ||
  index($0,"videoovermost") || index($0,"devp-iso") || index($0,"lsd") || index($0,"mm-ipod") ||
  index($0,"dmdt") { print }' >> "$R"

sect "MOST / video device nodes"
ls -l /dev/mlb/ 2>&1 >> "$R"
for P in /dev/mlb/isoTX1 /dev/mlb/isoTX2 /dev/mlb/isoRX1 /dev/mlb/isoRX2; do
  [ -e "$P" ] && say "present $P" || say "missing $P"
done
ls /dev 2>/dev/null | awk '/nv|tegra|hdmi|lvds|disp|fb/ {print}' >> "$R"

sect "display-manager mode markers (stock-only presence tests)"
for M in /var/dm_force_res /var/dm_force_res_aspect /tmp/dm_mode /var/dm_mode; do
  [ -e "$M" ] && say "present $M" || say "absent  $M"
done
ls /var 2>/dev/null | awk '/dm|display|kombi|hbas|lvds/ {print}' >> "$R"

sect "ports the project would use (must be free)"
netstat -an 2>/dev/null | awk '/6031|19820/ {print}' >> "$R"
say "(no lines above = nothing listening on 6031/19820)"

sect "CarPlay / PPS state (read-only)"
ls /ramdisk/pps 2>&1 >> "$R"
ls /ramdisk/pps/device 2>&1 >> "$R"

sect "existing project markers (must all be absent on a fresh unit)"
for M in /mnt/app/eso/lib/libmibr_carplay111.so /mnt/app/root/mibr-carplay111-backup /mnt/app/root/altscreen-u2; do
  [ -e "$M" ] && say "PRESENT $M" || say "absent  $M"
done

sect "done"
say "report: $R   (copy off with scp; contains no keys/tokens, but review before sharing)"
say "ALSO copy /ifs/lsd.jxe and the displaymanager binary (path from the process list above)"
exit 0
