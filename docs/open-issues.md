# Open issues and planned features

Owner's working order (2026-10-01): fps (Issue 2, fixed) → no-command start (Issue 3, done) → planned features → **Issue 1 (road-name bar) last**.

## Issue 1 — the native road-name bar, and the car's own "CarPlay route active" window

**Goal:** show the altscreen in the window the car switches to when CarPlay reports an active route; do not get kicked back to the native map when
the route ends; get rid of the bottom bar (altitude / road name / scale / compass) drawn by the cluster's own map page.

Findings [runtime unless marked]:
- With a CarPlay route active the car switches the cluster map area to another window which, with nothing else shown, displays only a **compass picture**.
- `sc 4 N` sweep with a route active: 72 native map, 73 blank, 74 native map, **76/77/78 show the altscreen — all still with the road-name bar**.
  The bar is therefore not drawn by the context's displayables.
- **The compass window is not a `dmdt` context [inferred, strong].** From the HMI's Java (decompiled `lsd.jxe` via jxe2jar + CFR): `GALHandler.updateNaviAppState(running_on_device)`
  → `ClusterService.updateGALState` → `CombiBAPListener.setGALState(true)` → `updateInfoStates()` sends BAP `INFO_STATES = 6` ("Navigation in mobile device active (DF 4.3)").
  The other info states are error/placeholder pages. The NavActiveIgnore jar stops this signal; with it enabled the compass never appears (confirmed on the car) and the
  road-name bar remains. **Current baseline: NavActiveIgnore enabled.**
- The bar is the cluster's own map page, filled by BAP functions sent from `CombiBAPListener`: `updateAltitude`, `updateCurrentPositionInfo` (road name),
  `updateCompassInfo`, `updateMapScale` (also `MAP_PRESENTATION`, `MAP_VIEW_AND_ORIENTATION`, `MAP_COLOR_AND_TYPE`).
- **Java experiment:** `MIBR-ClusterBar.jar` (patched `CombiBAPListener`, those four methods return immediately; tool `tools/BarPatch.java`) did **not** hide the bar —
  the cluster showed **0 values** — and the car's **navigation went into a crash state**; the jar was removed. Lessons: the ECU does not hide the bar when data stops; a
  jxe2jar-converted class is not a safe drop-in for the ROM class.
- Lead: the bar's size and position change with the dial layout, so the cluster knows the layout.
- **Upstream's contexts 80/81 are not the lever [static, 2026-10-02].** The HUD jar declares 80 = {97, 98, 101, 102, 33} and 81 = {97, 98, 101, 102, 96}
  (see [hud-vc-callers.md](hud-vc-callers.md)). They differ from 73–77 only by swapping displayable 20 for the jar's own 97/98 (maneuver tile + blank overlay);
  none of the displayables in them is the bar, and context 76 = {58} alone already shows the bar. Upstream's tile targets the small "well" beside the dial on
  a different cluster type, so it is no comparison for the large-map layout.
- **Where the bar probably comes from [inferred].** It stays with only layer 58 on display 4, so the cluster ECU draws it over the LVDS picture, driven by BAP
  (`INFO_STATES`, `MAP_PRESENTATION` — which the cluster sends *to* the head unit, `ALTITUDE`, position, scale, compass). Untested lever: other `INFO_STATES`
  values (0/1/2/3 are placeholder pages) and whether any of those pages omits the bar; the compass window seen with value 6 has not been checked for the bar.

Options [inferred — none tested]: keep NavActiveIgnore (current); patch `CombiBAPListener.setMapPresentation/setMapVisibility/setGALState` to announce "map visible, no
supplementary view"; look for a BAP `MAP_PRESENTATION` value without the bar.
Reversibility: `dc` contexts live in `displaymanager` memory; `sc 4 77` restores the working view.

## Issue 2 — frame rate and aliasing

- **Frame rate: fixed** ([TRACE-012](../traces/TRACE-012-framerate-features2.md)). The phone throttled stream 111 to ~15–17 fps because the alt display record had `features=0` and no input
  device; stage 7 (`display clone + virtual knob HID + primaryInputDevice`, `features=2`) restored full rate ("Test A solved the lag").
- Disproved on the way: renderer priority (`on -p 25`), Nagle/delayed ACK (`TCP_NODELAY`), loss in the forward path (hook counter showed the hook itself receiving ~16 fps).
- **Aliasing is Waze-only** — Apple Maps and Google Maps look fine on the same stream. Parked. Possible experiment: the physical size advertised in `/info` (200×75 mm).
- `dmdt ts 4 <file>` does not capture the video layer (transparent PNG apart from a few pixels); it did show display 4's raster is 1440×542.

## Issue 3 — no-command start: **done**

See [runtime-lifecycle.md](runtime-lifecycle.md). Untested: CarPlay surviving the car's own restart with the phone left plugged in; a teardown the phone makes by itself.

## Planned features

1. **Zoom with the scroll wheel** — done.
2. **View button → map adjusts** — done as dial-layout following (the hook never acts on View itself).
3. **HUD next turn** — experimental ([hud-route-guidance.md](hud-route-guidance.md)).
4. **Menu to switch back to the native map** — not started. Related to Issue 1: `dmdt sc 4 <ctx>` is the mechanism; unknown: the trigger, and how to stop the car switching
   back by itself.
5. **Disable the cluster's side menus while the altscreen is showing** — not started. Left button opens a list (Map, last destinations, favourites); right button opens
   map colours / display / orientation / auto zoom. Leads [inferred]: BAP `MAP_PRESENTATION` has `leftSideMenueOpen` / `rightSideMenueOpen` flags, plus DSICarKombi lists
   (`DCElementContentSelectionList…`, `kombifastlist`). Unknown: whether the head unit can refuse the open request or the ECU opens them locally.

## Housekeeping

Revert the tracer config; install r21; capture raw zoom/tracer logs; reconcile the source with the published tree ([builds.md](builds.md)).
