# What did not work

Interpretations that were wrong, experiments that failed, and the evidence that corrected them. Kept so nobody repeats them.

| # | Claim / attempt | What actually happened | Evidence |
| --- | --- | --- | --- |
| 1 | "The MU1440 path (H.264 → MPEG-TS → `isoTX2` → MOST) will carry the map" | This cluster's large map is LVDS; `outputDevice 1` + displayable 58 + context 77 is the working path | [display-path.md](display-path.md) |
| 2 | Stream 111 can listen on 6031 | Silent packet-filter drop on `carplay0` (phone `-6722` timeout); 6000 is allowed | [TRACE-006](../traces/TRACE-006-unit-packet-filter.md) |
| 3 | `dmdt sc 1 77` / `dm 1 …` | Display *id* is 4 (index 1 is printed by `gs`); those commands were no-ops and proved nothing | [TRACE-009](../traces/TRACE-009-display-manager-state.md) |
| 4 | Decode inside `dio_manager` (second NvSS) | Stock `NvSSVideoClose` hangs on leaving CarPlay → separate `vc_render` | [TRACE-010](../traces/TRACE-010-stock-a-b-ordering.md), [011](../traces/TRACE-011-vc-render-forward-mode.md) |
| 5 | Low fps from renderer CPU priority / Nagle / loss in the forward path | The phone throttled stream 111 (hook itself received ~16 fps); `features=0` was the cause | [TRACE-012](../traces/TRACE-012-framerate-features2.md) |
| 6 | Different `updateViewArea` parameters per view, animation 400 ms, `viewAreaTransitionControl`, different outer geometry for view 1 | Acknowledged (`status=0`) but no layout change; only `adjacentViewAreas` worked | [TRACE-013](../traces/TRACE-013-viewareas-turns.md) |
| 7 | Virtual knob HID report → zoom | Handler called, phone did not react; `changeMapZoomLevel` is the working route | [TRACE-015](../traces/TRACE-015-cluster-controls.md) |
| 8 | Map the View button / wheel press to a layout toggle | View is the MMI's own dial-size control; the toggle moved the card; wheel click left unmapped | [cluster-controls.md](cluster-controls.md) |
| 9 | "The route arrow sits at ~30 % across the safe area" | Held for the small dials only; in the big dials it did not predict the arrow | [safe-areas.md](safe-areas.md) |
| 10 | Copy all of the friend's MU1329 view-area values | Heights do not transfer (540 vs 456/542 canvas); card broke on the big dials; reverted | [safe-areas.md](safe-areas.md) |
| 11 | Spawn a shell / create a marker from the hook's teardown path (r35/r38/r39) | Coincided with `dio_manager` restarting; the whole-session branch is not always reached | [runtime-lifecycle.md](runtime-lifecycle.md) |
| 12 | Remove the cold-boot re-acquire (r36) | Cluster stuck on the native map | [runtime-lifecycle.md](runtime-lifecycle.md) |
| 13 | A fast path without the phone-present gate (r41) | Black after unplug | [runtime-lifecycle.md](runtime-lifecycle.md) |
| 14 | `/tmp/MMX-startup-finished` as a "navigation ready" gate | It exists at 00:00 — far too early | [runtime-lifecycle.md](runtime-lifecycle.md) |
| 15 | Child process spawned from the hook inherits harmlessly | It kept the hook's listening sockets (`EADDRINUSE`, fail-closed) → `FD_CLOEXEC` | [runtime-lifecycle.md](runtime-lifecycle.md) |
| 16 | `printf` on the unit to write a config | Not present; one redirect truncated a file → use `echo` | [ssh-environment.md](ssh-environment.md) |
| 17 | `dmdt` before/after comparison to find the dial layout | `gs`/`gd` print nothing about layout; the HMI's tracer carries it | [TRACE-015](../traces/TRACE-015-cluster-controls.md) |
| 18 | Patch `CombiBAPListener` to hide the road-name bar | Cluster shows 0 values; navigation went into a crash state; reverted | [TRACE-019](../traces/TRACE-019-road-name-bar.md) |
| 19 | Hide the compass window with a `dmdt` context | It is a BAP info state (6), not a context | [TRACE-019](../traces/TRACE-019-road-name-bar.md) |
| 20 | `rgd_rgtype=0` / `rgd_no_rgstatus` hide the cluster tile | The tile is fed by `pushClusterManeuver`; those markers are also cached at HMI start | [hud-route-guidance.md](hud-route-guidance.md) |
| 21 | The friend's Cinemo configuration applies to this unit | This unit has the HARMAN iAP2 stack (`mm-ipod`); there is no `CinemoDioManager.log` | [TRACE-018](../traces/TRACE-018-hud-route-guidance.md) |
| 22 | `rgd_hook` "takeover cancels the native route" | `Pre-gate: skipped — native cancel disabled`; the route was not cancelled | [hud-route-guidance.md](hud-route-guidance.md) |
| 23 | Replug reloads a swapped hook | A restart is needed to load a swapped hook | [runtime-lifecycle.md](runtime-lifecycle.md) |
| 24 | The `dio_manager` crash loop was caused by the hook | Not shown; a car restart cleared it | [TRACE-017](../traces/TRACE-017-dio-manager-triage.md) |
| 25 | Photo-based judgement of the turn card was reliable | Early readings of "clipped" vs "too big" were wrong; resolved by bisecting single values | [TRACE-016](../traces/TRACE-016-safe-area-tuning.md) |

Static-phase corrections (older, from the first analysis) are in [docs/static-phase/disproven.md](static-phase/disproven.md).
