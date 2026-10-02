# Repository status

> **Project:** MHI2 AltScreen — Audi MU0678 / B9 Virtual Cockpit
> **Platform:** Audi MHI2 `MU0678` (P3241), QNX 6.5, HARMAN iAP2 stack
> **State as of:** 2026-10-02
> **Purpose:** evidence-driven implementation of CarPlay secondary-screen navigation on the Virtual Cockpit.

## Current state

The CarPlay **second screen (stream 111)** is shown on the Virtual Cockpit of the reference car at ~29–30 fps.
It starts by itself after boot, returns to the native map when the phone is unplugged, and comes back on replug.
Steering-wheel zoom works, and the safe area follows the cluster's dial layout (two layouts verified).

This is a **developer proof of concept**. It is not an installer and has only been run on one car.

## What is proven (runtime)

| # | Finding | Trace |
| --- | --- | --- |
| 1 | Stock CarPlay = stream 110; the cluster map is a **second** stream, type 111, negotiated with `altScreen` + `viewAreas` | [001](traces/TRACE-001-gen1-hook-baseline.md), [005](traces/TRACE-005-iphone-side-negotiation.md) |
| 2 | The unit's `pf` ruleset drops inbound TCP on `carplay0` except 5000:5001, 5010, 6000, 6030, 7000:7001, 7100 — our 6031 listener timed out silently | [006](traces/TRACE-006-unit-packet-filter.md) |
| 3 | A native `Screen B` with a stub video sink receives stream 111 and reaches H.264 decode (SPS/PPS/IDR/slices counted) | [007](traces/TRACE-007-native-screen-b.md) |
| 4 | Rewriting B's `NvSSVideoOpen` to `outputDevice 1`, layer 58, plus `dmdt sc 4 77`, shows the picture on the cluster | [008](traces/TRACE-008-first-cluster-video.md), [009](traces/TRACE-009-display-manager-state.md) |
| 5 | B must start after stock stream A has configured; a 110-only teardown must not take 111 down | [010](traces/TRACE-010-stock-a-b-ordering.md) |
| 6 | A separate `vc_render` process fed over loopback avoids the `NvSSVideoClose` hang inside `dio_manager` | [011](traces/TRACE-011-vc-render-forward-mode.md) |
| 7 | The ~16 fps ceiling was the phone's choice (display record `features=0`); `features=2` + knob HID gives ~30 fps | [012](traces/TRACE-012-framerate-features2.md) |
| 8 | Two view areas switched on `turns`; the phone applies the switch only with `adjacentViewAreas` | [013](traces/TRACE-013-viewareas-turns.md) |
| 9 | Autostart, early/late plug, unplug → native map, replug | [014](traces/TRACE-014-autostart-persistence.md) |
| 10 | `changeMapZoomLevel` (`zoomDirection` 0 = in, 1 = out) zooms the CarPlay map; wheel events and dial layout come from the HMI's DSI tracer | [015](traces/TRACE-015-cluster-controls.md) |
| 11 | Final safe areas for small and big dials, and the measured rules behind them | [016](traces/TRACE-016-safe-area-tuning.md) |

## Experimental / open

| Item | State | Trace |
| --- | --- | --- |
| HUD next turn (route guidance) | data path observed working with a patched third-party hook; **package rolled back**; session drop at route end unexplained; cluster arrow still visible on the small dials | [018](traces/TRACE-018-hud-route-guidance.md) |
| Remove the native road-name bar | not solved; BAP-feed Java patch did not hide it and broke navigation (reverted) | [019](traces/TRACE-019-road-name-bar.md) |
| Menu to return to the native map | not started (wheel click deliberately unmapped) | — |
| Disable cluster left/right menus (keys 99/100) | not started | — |
| Third dial layout | slot and files exist; never exercised (not on the test car) | [015](traces/TRACE-015-cluster-controls.md) |
| Waze aliasing | Waze-only; Apple Maps and Google Maps are fine on the same stream | [012](traces/TRACE-012-framerate-features2.md) |
| `dio_manager` crash loop seen once | cause undetermined, not shown to be the hook | [017](traces/TRACE-017-dio-manager-triage.md) |
| Safe installer / other firmware | not started | — |

## Highest-value open questions

1. What closes the CarPlay session (USB re-enumeration, `mm-ipod` relaunch) a few hundred ms after a route ends
   when the route-guidance hook is loaded — and does it also happen with the hook disabled (`rgd_disable`)?
2. Is the small-dial arrow the cluster drawing the BAP maneuver descriptor itself (inseparable from the HUD feed), or a
   separate tile path?
3. Which BAP / DSI call opens the cluster's left (key 99) and right (key 100) menus, and can the head unit decline it?
4. Can the native road-name bar be hidden without breaking navigation (BAP map-presentation flags are the lead)?
5. Does the unit apply the layout from a third dial size, and what rectangle does the HMI announce for it?
6. Do other Audi MHI2 trains (and the 12.3″ cluster) accept the same hook gates? See [firmware identity](mu0678/firmware.md).

## Evidence rules

Every finding is classified as one of:

- **Proven (runtime)** — observed on the car, with a log or measurement in [`evidence/`](evidence/);
- **Proven (static)** — read from strings, symbols, config or disassembly;
- **Inferred** — consistent with evidence, not yet tested;
- **Unknown** — no evidence.

Disproven interpretations are kept in [docs/disproven.md](docs/disproven.md). Nothing is deleted when it turns out wrong.
