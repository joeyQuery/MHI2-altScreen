# Deployment and restore (developer path)

For someone who already has SSH access to the exact reference unit. **Not an end-user installer.** Replace `<unit>` with your unit's address.

## Gates (read-only first)

1. `ksh /tmp/mu0678_preflight_readonly.sh /tmp/sha256sum` — collects hashes and state; **proceed only if** `libairplay.so` is
   `a3ab0fe0ac20a712f7f240c1abd852a38faf78f2673880342d0eb24a6d8c29be`, the stock `smartphone_integrator.json` is `6e7968e0…`, no project markers are present,
   ports 6000/19820 are free and `/mnt/system` is remountable. See [mu0678/firmware.md](../mu0678/firmware.md).
2. Generate the patched `smartphone_integrator.json` on the workstation (`tools/make_patched_carplay_json.py`, hash-gated; the Audi array is multi-line so the
   Škoda awk patcher would corrupt it).

## Install

**First install** (config + hook, reversible, reboot required): `deployment/mu0678_phase3_install_r1.sh HELPER_SHA256SUM HOOK_SO PATCHED_JSON`. It refuses unless
`libairplay.so`, the hook and the patched JSON match their expected hashes, saves the stock `smartphone_integrator.json` under `/mnt/app/root/mibr-carplay111-backup/`,
installs `/mnt/app/eso/lib/libmibr_carplay111.so`, replaces the one config that makes `dio_manager` preload it, and tells you to reboot.
The script is **pinned to the first (r1) build's hash**: set `EXP_HOOK` (and `EXP_NEWJSON` if you changed the canvas) to the SHA-256 of the build you are installing,
e.g. `artifacts/mu0678/SHA256SUMS`, before running it.

**Later builds** (hook only, config untouched):

```sh
scp -O artifacts/mu0678/r60/libaltscreen111.so <helper sha256sum> deployment/mu0678_hook_swap.sh root@<unit>:/tmp/
ksh /tmp/mu0678_hook_swap.sh /tmp/sha256sum /tmp/libaltscreen111.so $(/tmp/sha256sum /tmp/libaltscreen111.so | awk '{print $1}')
```
`mu0678_hook_swap.sh` verifies the libairplay hash and the new hook's hash, backs the previous hook up as
`/mnt/app/root/mibr-carplay111-backup/libmibr_carplay111.so.prev-<sha>`, installs it, syncs and remounts read-only. **Restart the car** to load it
(a replug does not reload a swapped hook). Then place `vc_render` at `/mnt/app/root/vc_render`, the persistent markers (`mibr-carplay111-native-b`,
`mibr-carplay111-size`) and the safe-area files ([safe-areas.md](safe-areas.md)).

Other phase scripts: `mu0678_phase2_install.sh` (source-only capture test), `mu0678_phase2_capture.sh` (60 s H.264 capture from the tee), `mu0678_phase2_restore.sh`.

## Restore

- Hook: copy the saved `libmibr_carplay111.so.prev-<sha>` back (same swap script with the old file), restart.
- Configuration: `mu0678_phase2_restore.sh HELPER_SHA256SUM` restores the stock `smartphone_integrator.json` from the verified backup and, only once the stock config is confirmed, removes the hook; restart.
- Cluster: `dmdt sc 4 72` returns display 4 to the native map immediately.
- HUD: `sh /mnt/app/root/hud/hud_rollback.sh`, restart.
- Tracer: restore `traceConfig.properties.orig`, remove `dsitracer_activated`, restart.

Test with the car parked, one command per paste, and keep a way in that does not depend on the screen.
