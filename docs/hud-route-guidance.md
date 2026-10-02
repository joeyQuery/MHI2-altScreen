# HUD route guidance (experimental)

**Goal (owner):** the HUD shows the next turn from CarPlay. The cluster's own maneuver tile is not wanted on the small dials (acceptable on the
big dials).

**State on 2026-10-02:** the HUD data path was observed working; the package is **rolled back** on the car; a CarPlay session drop at route end is
unexplained; the cluster still shows an arrow on the small dials. Raw evidence: [TRACE-018](../traces/TRACE-018-hud-route-guidance.md).

## Why a new component was needed

- The head unit's `dio_manager` on this car is the **MHI2Q (HARMAN iAP2) variant**, not the Cinemo one. `/ramdisk/pps/iap2` only exposes
  `device, location, nowplaying, telephony`; `grep -i -e guid -e route -e maneuv /etc/eso/production/dio_manager.json` finds nothing; and the HMI's
  `DSICarplay` has **no route-guidance attribute** — the HMI never receives maneuvers natively. [runtime]
- The iAP2 identification must announce a **RouteGuidanceDisplay** component and the unit must then parse the phone's `0x5201/0x5202/0x5204`
  messages. That is exactly what `chefranov/mhi2-au37x-carplay` does for the same HARMAN stack.

## What the upstream package does (studied, not redistributed)

`mhi2-au37x-carplay` @ `8968119c136b1f148486149de0950058829a2f33` ([hud/UPSTREAM.md](../hud/UPSTREAM.md)):

- `librgd_hook.so` is `LD_PRELOAD`ed into `mm-ipod` through a shim, hooks the `ipod-drvr-iap2.so` vtable, adds the RouteGuidanceDisplay component to the
  identification, sends `0x5200`, parses `0x5201/0x5202/0x5204`, serves Java on `127.0.0.1:19810` and logs to `/mnt/app/rgd_hook.log`.
- `rgd_hook.jar` replaces the stock `ClusterService`/`DisplayManagerMIB2High` and drives BAP (`CombiBAPServiceNavi`): `MANEUVER_DESCRIPTOR`,
  `DISTANCE_TO_NEXT_MANEUVER`, `TURN_TO_INFO` for the HUD **and** a separate cluster-tile path (`pushClusterManeuver` → `ClusterService.updateNextManeuver`).
  Class files are version 46 (Java 1.2, no stack maps), so byte patching is safe. Its Java log is `/tmp/carplay_hook.log`.
- It also ships its own renderer (`maneuver_render`), cover-art and D-pad hooks; none of that is used here.

## The HUD-only package

[`hud/hud_install.sh`](../hud/hud_install.sh) (POSIX sh; the unit has no `sed/awk/printf/dirname`) installs `librgd_hook.so` and `rgd_blank.png` into
`/mnt/app/eso/hmi/lib`, saves `mm-ipod` as `rgd_real/mm-ipod` and `mm-ipod.orig`, writes the shim
```sh
[ -f /mnt/app/rgd_disable ] || LD_PRELOAD=/mnt/app/eso/hmi/lib/librgd_hook.so
export LD_PRELOAD
exec /mnt/app/armle/usr/sbin/rgd_real/mm-ipod "$@"
```
and installs the patched jar **last** (it is the only part that can stop the HMI starting). [`hud/hud_rollback.sh`](../hud/hud_rollback.sh) removes the jar,
restores `mm-ipod` from `.orig` and deletes the hook files. Off switch without rollback: `touch /mnt/app/rgd_disable` (replug for the shim, restart for Java).

[`tools/patch_rgd_hook_jar.py`](../tools/patch_rgd_hook_jar.py) applies three byte patches to the upstream jar (each asserted to match once):

| # | Patch | Reason |
| --- | --- | --- |
| 1 | `ScreenModule.ctxEnabled()Z` → `iconst_0; ireturn` | the jar must never switch cluster display contexts (it ships ctx 80/81 logic that would take the cluster from the altscreen) |
| 2 | `BAPBridge.pushClusterManeuver(II)V` → `return` | stop feeding the cluster's maneuver tile; the BAP/HUD path is untouched |
| 3 | `RouteGuidance` constant `6000L` → `3000L` (`DEFERRED_SHUTDOWN_MS`) | the HUD took ~6 s to clear after the route ended |

Patch 1 alone was **v1** (157574 bytes in the build used); patches 1–3 are **v2** (157577 bytes). The script reproduces the v2 class contents
byte for byte (zip metadata differs).

## Findings

- **HUD works** with v1: owner report "The HUD works perfectly". The log shows `RG presentation CONFIRMED: BAP snapshot published`, then a stream of
  `bap: distToManeuver …` / `maneuverState` lines and `BAP maneuver descriptor: count=1 symbol=6 dir=0`. [runtime]
- **The cluster tile is a separate path** from the HUD's BAP descriptors. `RGStatus`/`ActiveRGType` suppression (`rgd_no_rgstatus`, `rgd_rgtype`) did not hide it;
  `pushClusterManeuver` is what feeds it. Several markers are cached at HMI start (`rgd_no_rgstatus`, `rgd_disable`, `rgd_no_overlay`), `rgd_rgtype` is re-read every 3 s.
  Silencing it with patch 2 did **not** remove the arrow the owner still sees on the small dials, and the v2 log shows no `cluster tile:` pushes during the route — so
  what remains is most likely the cluster drawing the **BAP `MANEUVER_DESCRIPTOR` itself**, which is the same data the HUD needs. [inferred]
- **"Takeover" wording is misleading.** `REPLACE: takeover engaged (stock nav cancelled, RG gate shut)` is logged, but `Pre-gate: skipped — native cancel
  disabled (touch /mnt/app/rgd_cancel_native to enable)` shows the native route was **not** cancelled. `NavStatusGate/GatedCombiService` does block native
  BAP route-guidance/position info to the cluster while engaged. `Shutdown: built-in navigation still holds a route` appears at every end.
- **Shutdown path errors:** `sync threw java.lang.NoSuchMethodError: de/audi/app/bap/fw/functionsync/IFunctionSynchronizationHandler` — the jar's API does
  not match this HMI at that call; several `Shutdown (full teardown)` cycles follow each route end.
- **CarPlay session drop at route end (open).** In the last run the route ended at jar-log 301.6 s; `GEN2 TEARDOWN … reason=-6753` followed at 301.765 s;
  `mm-ipod` was relaunched at ~304 s (`CarplayBus hook connected` at 305.0 s; altscreen hook restart at 305.4 s). The slog shows the USB device relaunching
  `mm-ipod`, **no SIGSEGV**, so mm-ipod did not crash — the phone detached/re-attached. The `rgd_hook.log` pool lines sat at `recv 5/6 send 5/6` throughout and
  `reclaiming packets the driver drops: rc=22 first_msg=0x5201` appeared. Whether this also happens with the hook disabled was **not** tested.
- The stock navigation showed odd state after the restart (route preloaded in the cluster, no position in MMI navigation). Not attributed to the jar
  (native cancel was off); not reproduced.
- After the rollback CarPlay did not come up until the MMI was restarted again; before that the unit showed no `vid=1a96` USB device and no `dio_manager`
  (the phone had simply not enumerated). See [TRACE-017](../traces/TRACE-017-dio-manager-triage.md).

## Next steps

1. Cold start with `rgd_disable` set; start/stop a route; does the session still drop? (isolates the hook).
2. Reinstall v2 with the phone not previously navigating; confirm HUD data and the 3 s clear.
3. Decide on the small-dial arrow: accept it, or find where the cluster draws it from (BAP `MAP_PRESENTATION` / info states).
4. Patch the `NoSuchMethodError` shutdown call or avoid it, if it is related to the repeated teardown cycles.
