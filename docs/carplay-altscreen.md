# CarPlay AltScreen protocol notes (as observed on this car)

Status tags: **[runtime]** seen in logs, **[phone]** seen in the iPhone's unified log, **[static]** read from strings/disassembly.

## Streams

- **Stream 110** is the normal CarPlay screen. **Stream type 111** is the alternate (cluster) screen. [runtime]
- The phone opens two screen streams for one session (`0x58D2` main, `0xACDD` alt) — [TRACE-005](../traces/TRACE-005-iphone-side-negotiation.md). [phone]
- A normal navigation-app or navigation-ownership change does not require tearing down and recreating stream 111.
  A partial AirPlay `TEARDOWN`, a fresh SETUP, a changed connection ID or socket death does. [runtime/inferred]

## Feature keys

The phone proposes `uiContext, viewAreas, cornerMasks, focusTransfer, h.264Level5.1, mainBuffered, altScreen, enhancedSiri,
hevc, vehicleStateProtocol, sessionManagement, logTransfer, iAPChannel` and **negotiates only `altScreen` and `viewAreas`**
when the head unit advertises them. [phone]

## Cluster URLs and `suggestUI` / `showUI`

The phone supports three cluster contexts: `maps:/car/instrumentcluster`, `…/map`, `…/instructioncard`. They are logical UI
contexts inside the same stream, not separate streams. [phone]

- `suggestUI` is a suggestion, not a switch. On this unit the stock handler answers `-6714` (not handled); the hook
  acknowledges it and keeps the URL list as candidates. [runtime]
- The phone then sends `showUI` for our UUID (`"Unhandled command 'showUI' … forwarding to clients"` — it is the *accessory*
  that must select the UI) and activates the `DBInstrumentCluster` scene. [phone]

## The display record the phone is given

Stock main display `/info` record: `widthPixels=1024, heightPixels=480, widthPhysical=188, heightPhysical=88, maxFPS=30,
features=2, uuid`. [runtime]

Our alt record: type 111, `maxFPS 30`, 1440×456 (declared canvas 1440×542), physical 200×63–75 mm, `initialURL`,
`altScreenSuggestUIURLs`, `viewAreas`, **`features=2` and a primary input device** (a cloned `Eso_Knob` HID). With
`features=0` the phone throttled stream 111 to ~16 fps ([TRACE-012](../traces/TRACE-012-framerate-features2.md)).

## View areas

`viewAreas` carries a list of `{viewArea, safeArea}` entries and an `initialViewArea`. The hook switches between them with
`updateViewArea` on the phone's `turns` field (route guidance on/off). **The phone only applies the switch when the command
also carries `adjacentViewAreas`** — an integer array naming the other views (mode 1 = all the others). Without it every
command was acknowledged (`status=0`) but the first layout stayed. [runtime] ([TRACE-013](../traces/TRACE-013-viewareas-turns.md))

Safe-area behaviour (where the arrow and turn card land) is in [safe-areas.md](safe-areas.md).

## Zoom

The CarPlay session command `changeMapZoomLevel` takes `zoomDirection`: **0 = zoom in, 1 = zoom out** (−1 also zooms out;
2 does nothing). The command name was found in a different head unit's cluster library (strings, next to `updateViewArea`,
`showUI`, `stopUI`) and tested here with marker files before wiring the wheel. [static + runtime]
([TRACE-015](../traces/TRACE-015-cluster-controls.md))

## HID

`HIDDeviceCreateVirtual / HIDRegisterDevice / HIDPostReport(uuid, …)` accept a cloned knob device and the stock handler is
called, but the phone did not react visibly on the cluster. The virtual-knob route is **not** used for zoom
([docs/disproven.md](disproven.md)).

## Teardown reasons

`GEN2 TEARDOWN entry request=NULL reason=-6753` marks the end of a session (unplug, or the phone dropping the link). The hook's
watcher counts these lines to know the phone has gone ([runtime-lifecycle.md](runtime-lifecycle.md)).
