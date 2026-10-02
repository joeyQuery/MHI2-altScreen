# What the HUD package puts on the Virtual Cockpit — audit

**Date:** 2026-10-02. **Method:** decompiled upstream `rgd_hook.jar` (commit in [hud/UPSTREAM.md](../hud/UPSTREAM.md)); `javap` comparison of the two
HMI classes the jar replaces against **this unit's** stock classes (converted from the unit's `lsd.jxe`); strings of `librgd_hook.so`; the v1/v2 logs in
[TRACE-018](../traces/TRACE-018-hud-route-guidance.md). Nothing here was run on the car in this session. [static] = read from code, [runtime] = seen in a log.

## Short answer

With the HUD-only patches, the things that reach the cluster are **BAP messages** (maneuver descriptor, distance, turn-to text, destination, lanes, position
text, route-guidance status) and a small set of **replaced HMI classes**. The jar's own cluster-drawing machinery (display contexts 80/81, KDK backings, render
window, graphics-available announcement) is **inert** because patch 1 turns `ctxEnabled()` off and no renderer process exists. The arrow still seen on the small
dials is therefore the cluster drawing the BAP maneuver descriptor itself.

## Inventory

| # | Path | Where | What it does at the cluster | HUD-only state |
| --- | --- | --- | --- | --- |
| 1 | BAP `MANEUVER_DESCRIPTOR` | `BAPBridge.sendManeuvers` → `updateManeuverDescriptor` | The symbol the HUD shows **and** the cluster can draw. Upstream blanks it (`suppressBapSymbol()`) only while the jar owns the cluster (`ScreenModule.ownsCluster()`) so its own renderer's arrow is not doubled. | **Active** — `ownsCluster()` is false, so the real symbol is sent [static]; log `BAP maneuver descriptor: count=1 symbol=6 dir=0` [runtime] |
| 2 | Cluster tile | `BAPBridge.pushClusterManeuver` → `ClusterService.updateNextManeuver` / `updateRGIString` | Separate path to the cluster's maneuver tile | v1 log shows `cluster tile:` pushes [runtime]; **patched out in v2** |
| 3 | Distance, turn-to text, destination, lanes, position text, time | `BAPBridge` (`sendDistanceToManeuverRaw`, `updateTurnToInfo`, `updateDestinationInfo`, lane and scroll code) | BAP data for the cluster/HUD | Active |
| 4 | Route-guidance status | `updateRGStatus(1)`, `updateActiveRGType`, `forceClusterRouteInfoState` (`setRgActive`, `setRgiDataValid`) | Tells the cluster a route is active | Active unless `/mnt/app/rgd_no_rgstatus` |
| 5 | Native-RG gate | `NavStatusGate` / `GatedCombiService` | Blocks the **native** navigation's BAP route-guidance/position while engaged | Active |
| 6 | Replaced HMI classes | `de.audi.tghu.fwhmi.DisplayManagerMIB2High`, `de.audi.tghu.navi.app.cluster.ClusterService` in the jar | Override the stock classes of the same name from `lsd.jxe` | **Active** (see below) |
| 7 | Cluster contexts, KDK backings 101/102, render window, phone-map stage worker | `ScreenModule` | Switches the cluster to contexts 80/81 and positions the renderer window | **Inert**: `ensureContextWorker()` and `ownsCluster()` return early when `ctxEnabled()` is false [static] |
| 8 | Graphics-available / KOMO announcement | `BAPBridge.announceGraphics`, `forceGfxAvailable` | Sets `gfxAvailable` and KOMO data rate | **Inert**: needs `ownsCluster()`, the renderer's first frame, or `/dev/shmem/altscreen_live` (our hook never writes it) [static] |
| 9 | Renderer socket | `RendererServer` | Listens on 127.0.0.1:19800 for upstream's `maneuver_render` | Listens, nobody connects [runtime: `listening on 127.0.0.1:19800`, `CR: stopped`] |

## The replaced HMI classes (row 6)

Both are decompiled from upstream's car (an A3 on a different HMI build) and shadow this unit's stock classes. Compared with the stock classes on MU0678:

- `DisplayManagerMIB2High` — **adds** displayable 98 (an image displayable from `/mnt/app/eso/hmi/lib/rgd_blank.png`), contexts **80 and 81**, a `switchContext`
  gate and skin tracking. Contexts 72–78 (the ones the altscreen uses, including 77 = {20,102,101,58}) are **identical** to stock.
  **Drops** a stock context **34** ({16, 50, 51}) — upstream's copy does not declare it. What displayables 50/51 are was not established; this is unverified.
- `ClusterService` — **adds** `setRgiDataValid`, the "GAL" hold (`setGalStateHeldDown`, `updateGALState` held down), `getDSIResponseContainer`,
  `triggerRefreshRGIValid`, logging helpers. The GAL hold only runs with `/mnt/app/rgd_stock_info` present or while the jar owns the cluster.
- All other method bodies differ only by decompile/convert noise (the stock classes come from a J9 ROM image).

## Image handling

- Nothing in the HUD-only path fetches or decodes pictures from the iPhone. Cover art is a separate upstream feature (`coverart_hook.jar`, `libcarplay_hook.so`,
  cluster coding module 17 `Picture_Upload_Download`); **none of it is installed** by [`hud/hud_install.sh`](../hud/hud_install.sh). It is on the
  [roadmap](../ROADMAP.md) for later.
- The only image-related items the package adds are `rgd_blank.png` and displayable 98 (row 6). If CarPlay's image display misbehaves with the package
  installed, those and the missing context 34 are the first things to check.
- **`rgd_blank.png` is a gate, not decoration.** `BAPBridge.isPresentationReady()` returns `staticOverlayReady()`, which needs `overlayAvailable`, which is set
  only when that file exists. `RouteGuidance` will not confirm a presentation (`RG presentation CONFIRMED`) without it. Deleting the file would stop the HUD.
  A fifth byte patch (`staticOverlayReady()` → true) would remove that dependency, after which the file, displayable 98 and contexts 80/81 could all be left
  out. Not built or tested.

## Consequences for the small-dial arrow

The arrow comes from row 1. Options, from least to most invasive: (a) blank the descriptor symbol on the small dials only
([`tools/HudJarPatch.java`](../tools/HudJarPatch.java) patch A — untested; may blank the HUD too if it reads the same field); (b) find the BAP state that
decides whether the cluster draws a maneuver on the small dials. Neither has been run on the car.

## Which HMI classes the jar should and should not override (linkage audit)

[`tools/LinkCheck.java`](../tools/LinkCheck.java) resolves every method and field the jar references against this unit's stock classes (`lsd.jxe` converted to a jar).
The jar contains 61 classes; **exactly two stock classes are overridden** (`ClusterService`, and `DisplayManagerMIB2High` with its two inner classes). Everything else is new
`com.mhi2.*` code. Only **two** references fail to resolve on this unit:

| Unresolved reference | From | Consequence |
| --- | --- | --- |
| `Util.formatDistance(int,int,int,String)` (the unit has the 2-arg and the 5-arg overloads only) | the jar's `ClusterService.updateDistanceToDestination(int,boolean)` | `NoSuchMethodError` whenever a distance to destination is updated — **native navigation** hits this, not only CarPlay. Likely contributor to the odd navigation state seen after the HUD test (not proven). |
| `IFunctionSynchronizationHandler.isSyncCancelled()` | `BAPBridge.logClusterState` | The `sync threw NoSuchMethodError` line at every shutdown. A diagnostic dump; harmless. |

**`DisplayManagerMIB2High` — should not be overridden.** The HUD needs nothing from it. It adds contexts 80/81 and displayable 98, drops stock context 34, and gates
`switchContext`. Its only effect on the HUD is the `rgd_blank.png` "presentation ready" gate (above), which a one-method byte patch removes. Without the override the
display manager is exactly stock.

**`ClusterService` — needed, but not as upstream shipped it.** It is the jar's bootstrap: its `setCombiBAPService` hands the BAP service to `NavStatusGate` and starts
`RgdService`, and it exposes `getCombiBAPListenerCombiService` (and, for optional features, `setRgiDataValid`, `getDSIResponseContainer`, `rgdRefreshManeuverDistance`, the GAL hold).
A source-level diff against the stock class shows these *other* differences that change native behaviour on this car:

| Difference from stock | Effect | Action |
| --- | --- | --- |
| 4-arg `Util.formatDistance` (above) | breaks distance-to-destination | patch to the 5-arg overload |
| `satMapProviderChanged = true` and `setOnlineNavigationState(n==1 && !changed)` | online-navigation state reported as false; nothing on this unit clears the flag | default the flag to false (restores stock) |
| `updateRgActive(false)` also calls `updateDestinationInfo(null,0,0)` | clears BAP destination info when guidance ends | keep (it helps the HUD clear) but note it also applies to native routes |
| `onMagnificationChanged` → `ScreenModule.onRollerZoom` | no-op without `/dev/shmem/altscreen_live` | harmless |
| `getInvalidDistance("","")` → `""` | identical result | none |
| logging helpers, GAL hold | inert unless `rgd_stock_info` | none |

**v4 jar (built 2026-10-02, not yet run on the car):** [`tools/HudJarPatch.java`](../tools/HudJarPatch.java) applies, after the three byte patches of `patch_rgd_hook_jar.py`:
A small-dial symbol blanking, B `staticOverlayReady()` → true, C the 5-arg `formatDistance`, D `hasSatMapProviderChanged()` → false, E the three `DisplayManagerMIB2High*` classes
removed. `rgd_blank.png` is no longer installed. Re-running `LinkCheck` on the v4 jar leaves three unresolved references: the diagnostic `isSyncCancelled()` and two references to the
*stock* `DisplayManagerMIB2High` (`currentSkin()` inside a `try/catch Throwable`, and `isManeuverOverlayAvailable()` in `ScreenModule` code that returns earlier while `ctxEnabled()` is false) —
neither is reachable in the HUD-only path.
Replacing `ClusterService` with a converted copy of the unit's own class was considered and rejected: an earlier converted-class experiment crashed navigation
([open-issues.md](open-issues.md)), so a javac-built class with two verified byte fixes is the lower risk.
