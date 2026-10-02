# Architecture

Two problems have to meet on this car:

1. **CarPlay must create and maintain a secondary navigation presentation** (stream type 111).
2. **The head unit must put that video where the cluster reads it** and arbitrate it against the stock map producer.

Media transport and UI ownership are related but not identical: a valid stream-111 H.264 stream is not by itself
the stock secondary-screen behaviour.

## Components

| Component | Where it runs | Role |
| --- | --- | --- |
| `libaltscreen111` (the hook) | `LD_PRELOAD` into `dio_manager` (started by `smartphone_integrator`) | advertises the alt display, accepts stream 111 through a native `Screen B`, forwards access units, handles view areas, zoom, layout, autostart |
| `vc_render` | own process, spawned by the hook | owns the cluster's NvSS instance (`outputDevice 1`, layer 58) and decodes the forwarded frames |
| `displaymanager` | stock | composes display 4 (the cluster) from displayables placed in contexts; we only call `dmdt sc 4 77` / `72` |
| HMI (`lsd.jxe`, Java) | stock | cluster BAP feed, key events, layout; observed through the DSI tracer, never modified in the baseline |
| `mm-ipod` + iAP2 | stock | USB device stack; only the HUD experiment touches it |

## Data flow

```text
iPhone ─ USB ─► ipod/iAP2 ─► dio_manager
                              ├─ stream 110 ─► stock Screen A ─► NvSS (main display)           [unchanged]
                              └─ stream 111 ─► Screen B (stub video sink)
                                               └─ H.264 AU ─► 127.0.0.1:19830 ─► vc_render
                                                               NvSSVideoOpen(outputDevice 1, layer 58)
displaymanager: display 4 ── context 77 {20 MAP_ROUTE_GUIDANCE, 102, 101, 58 stream} ─► cluster
                display 4 ── context 72 {33 KOMBI_MAP_VIEW}                           ─► native map (fallback)
```

## Control plane (what the phone is told)

The hook rewrites the head unit's `/info` so the phone sees a second display (`type 111`, `1440×456`/`1440×542` canvas,
`features=2`, `viewAreas`, `altScreenSuggestUIURLs`). The phone then negotiates `altScreen` + `viewAreas`, creates its
`DBInstrumentCluster` scene for our display UUID and opens a second screen stream. See
[carplay-altscreen.md](carplay-altscreen.md).

## Why a second process

Opening a second NvSS video instance inside `dio_manager` made the stock main-screen `NvSSVideoClose` hang when leaving
CarPlay. `vc_render` owns the cluster instance instead, so `dio_manager` never holds more than the stock one
([TRACE-010](../traces/TRACE-010-stock-a-b-ordering.md), [TRACE-011](../traces/TRACE-011-vc-render-forward-mode.md)).

## What this architecture deliberately does not do

- It does not send MPEG-TS to MOST. This cluster's large map is carried over LVDS ([display-path.md](display-path.md)).
- It does not replace the stock navigation. The native map stays the fallback and returns on unplug.
- It does not modify the HMI Java in the baseline. The Java experiments are documented separately and are not part of it.
