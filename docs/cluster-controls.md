# Cluster controls and dial layouts

Hook builds r47–r60. Tags: **[runtime]**, **[inferred]**, **[unknown]**. Raw captures of the tracer lines were not archived; the findings
below were recorded in the project notes at the time ([TRACE-015](../traces/TRACE-015-cluster-controls.md) lists what is and is not backed by a log).

## Feature status

| Feature | Status |
| --- | --- |
| Scroll wheel zooms the CarPlay map | done (r52); small delay, accepted |
| Dial layouts (View button changes the cluster gauge size) | done for 2 layouts (r55/r56/r58/r59/r60); third untested |
| HUD next turn | experimental — [hud-route-guidance.md](hud-route-guidance.md) |
| Menu to return to the native map | not started; wheel click deliberately left unmapped |
| Disable VC left (key 99) / right (key 100) menus | not started |

## Reading the car's key and layout events (DSITracer) [runtime]

The HMI has a tracer that can stream DSI calls. `/mnt/app/eso/hmi/lsd/traceConfig.properties` (original saved as `.orig`) scoped
`DSIKeyPanel`, `DSIKOMOView` and `DSIMapViewerZoomEngine` to full; the marker `dsitracer_activated` sits beside it; `use.appender=telnet,bluepirat`.
A restart is needed, and the tracer slows HMI boot slightly. To revert: restore `.orig`, remove the marker.

| Appender | Port | Notes |
| --- | --- | --- |
| telnet | 1798 | live only, no replay |
| bluepirat (BufferedTelnetAppender) | 20991 | the hook connects ~20 s after boot and sees the boot-time layout announcement |
| client (binary) / ctrl | 1800 / 1797 | not used |

Live line: `1#org.dsi.ifc.keypanel.DSIKeyPanelListener#updateEncoder2#<ts>|int:4|int:40|int:±1|int:0|int:1`. The leading digit is the DSI
instance (`1#` and `5#` duplicate). Bluepirat form: `<seq>?<instance>#class#method#ts|args@`.

| Input | Event |
| --- | --- |
| scroll-wheel notch | `updateEncoder2`, keyboard 4, key/encoder **40**, ±1 |
| **View** button | `updateKey2(4, 35, state)` |
| wheel press | `updateKey2(4, 40, state)` (1 pressed, 0 released, 3/4/5 long) |
| VC left menu | key **99** |
| VC right menu | key **100** |

> **The View button is the MMI's own gauge-size control. The hook never acts on it** (owner correction). Mapping the wheel press to a layout
> toggle was tried and rejected: it moved the card and the centre.

## Zoom [runtime]

`changeMapZoomLevel`, `zoomDirection` **0 = in, 1 = out** (−1 also out, 2 nothing), sent through the hook's diagnostic-command dispatcher.
r52 reads wheel events from the tracer (port 1798): `+1` → in, `−1` → out (swap marker `/tmp/mibr-alt111-zoom-swap`). Test marker:
`/tmp/mibr-alt111-zoom` containing `<dir> [nouuid]`. Observed: zoom works on the CarPlay map and on the MMI's native map rectangle; a fraction-of-a-second delay.

The virtual-knob HID route (r45–r49: clone `Eso_Knob`, `HIDPostReport`) reached the session handler (once registered before the phone connected,
`rc=0`; `-6727` when the device was not registered this session) but the phone did not react visibly — not used.

## Cluster dial layouts [runtime]

The HMI sends `DSIMapViewerZoomEngine#setZoomArea` (instance 1) when the layout changes; instance 0 (`0:10:1024:422`) is the main MMI map and is ignored.

| Layout | Rect `x:y:w:h` | Hook slot |
| --- | --- | --- |
| Small dials | `370:49:700:300` | 0 |
| Big dials | `490:49:460:300` (visible window x 490–950, centre 720) | 1 |
| Third layout | not available on the test car | 2 |

Slot rule: `Rect width ≤ 560 → 1`, `≤ 800 → 0`, else `2`. Each slot has an idle and a route view (up to 6 view areas, `ALT111_MAX_VIEWS = 6`).
Files: `mibr-carplay111-safearea[-idle].conf` (slot 0), `…-narrow-idle|route.conf` (slot 1), `…-full-idle|route.conf` (slot 2). A slot is only
created when **both** its files load. Log: `r55 zone: setZoomArea Rect=…`, `r56 layout -> slot N (Rect width W) viewAreaIndex=… intent rc=…`.
Raw lines: [evidence/logs/015-layout-detection.txt](../evidence/logs/015-layout-detection.txt).

## The friend's table (a different head unit) [static]

A package for a MU1329 (Cinemo) unit stores three 16-byte `(x, y, w, h)` entries at file offset `0xbb50` of its `libmhi2qcarplaycluster.so`:

| View | x | y | w | h |
| --- | ---: | ---: | ---: | ---: |
| 0 small dials | 360 | 87 | 720 | 297 |
| 1 big dials | 520 | 87 | 400 | 297 |
| 2 third layout | 114 | 88 | 350 | 296 |

Its display record: type 111, maxFPS 60, 1440×540, physical 320×120, `features 2`, `primaryInput 3`; other keys `drawUIOutsideSafeArea`,
`viewAreaTransitionControl`, `initialViewArea`, `hidDevices` (`Eso_Knob`, `MHI2Q_ClusterKnob`). The heights do not transfer to this canvas
(456 usable, 542 declared). Copying all of its values was tried on the car and reverted ("not the greatest experience") — [safe-areas.md](safe-areas.md).

## Other notes

- One `dio_manager` crash loop was seen (`run fault … ip 0x103e4b4`, restart every 20–60 s, USB ports unresponsive); a car restart cleared it. Not shown to be
  caused by the hook ([TRACE-017](../traces/TRACE-017-dio-manager-triage.md)).
- QNX shell: no `printf`, `telnet`, `nc`, `strings`, `diff`, `cmp`; `grep` has no `-B` and cannot search binaries; use `echo` to write config files
  (`printf` truncated a file once) — [ssh-environment.md](ssh-environment.md).
- Cold-boot black time was reduced in r48 (forward retry 250 ms, keyframe wait 500 ms, `NB_FWD_FK_MAX` 3).
