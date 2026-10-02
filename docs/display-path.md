# Display path on the MU0678

## Result

The cluster is display **4** in `displaymanager.json` (`2_lvds`, terminal `Tegra:HDMI0`, 1440×542). It is fed by **NvSS
`outputDevice 1`**, and a **stream-type displayable** (id 58) placed in context 77 shows the video. `dmdt sc 4 77` shows it;
`dmdt sc 4 72` restores the native map. [runtime] ([TRACE-008](../traces/TRACE-008-first-cluster-video.md), [TRACE-009](../traces/TRACE-009-display-manager-state.md))

## Inventory (live `dmdt gs / gc / gd`, CarPlay connected)

| display | terminal | size | active context | displayables |
| --- | --- | --- | --- | --- |
| 0 | `main` (`Tegra:TFTLCD0`) | 1024×480 | 78 | 16 HMI, 59 EXTERNAL_SMARTPHONE |
| 1 (id **4** in config) | `2nd_lvds` (`Tegra:HDMI0`) | 1440×542 | 74 | 20 MAP_ROUTE_GUIDANCE, 102, 101, 33 KOMBI_MAP_VIEW |

Note the numbering trap: `dmdt gs` prints the display *index* (1); the switch command takes the *display id* from the
configuration (**4**). Early `sc 1 …` attempts targeted a non-existent display and were no-ops — they proved nothing
([disproven.md](disproven.md)).

Displayables: 16 HMI window 1024×480 · 19 MAPVIEWER 1024×436 · 20 MAP_ROUTE_GUIDANCE 328×180 · **33 KOMBI_MAP_VIEW 1440×455** ·
18 BROWSER · 51 STREETVIEW · 101/102 backgrounds 210×153 · **59 EXTERNAL_SMARTPHONE** (stream, 0 buffers). Displayable **58
GOOGLE_EARTH_KOMBI_MAP_VIEW** is not in this list of nine; it appears in contexts 76/77 and becomes a live stream displayable once our
NvSS open uses it (it was registered twice, once by our stream and once by a stock window, and still worked).

## Contexts on the cluster display (87 contexts in total)

| ctx | contents | note |
| --- | --- | --- |
| 72 | {33} | native map (our fallback) |
| 73 | {20, 102, 101} | blank |
| 74 | {20, 102, 101, 33} | stock active (native map + route-guidance overlay) |
| 75 | empty | `sc 4 75` does nothing |
| 76 | {58} | altscreen |
| **77** | {20, 102, 101, **58**} | **altscreen, the one used** |
| 78 | {16 HMI, 59} on display 0 (the main display's active context); `sc 4 78` also showed the altscreen | — |

`sc 4 76/77/78` all show the altscreen; **all of them still show the road-name bar**, so the bar is not drawn by the
context's displayables ([TRACE-019](../traces/TRACE-019-road-name-bar.md)). `dc 74 20 102 101 58` is accepted and redeclares an
existing context in memory (lost when `displaymanager` restarts).

## NvSS facts the hook relies on [static]

- `NvssVideoImpl::start` reads `NVSS_VIDEO_OUTPUT_DEVICE` on every call (non-zero → output 1) and hard-codes **layerID 59**
  (= the displayable id of EXTERNAL_SMARTPHONE). NvSS layerID **is** the displayable id.
- `NvSSVideoOpen` is an imported PLT symbol (GOT slot), so `LD_PRELOAD` interposition works; the open-params struct is 0x18 bytes
  (`outputDevice`, …, layer byte at +0x0a).
- Only B's open is rewritten (`outputDevice 0→1`, layer `59→58`). Stream A is never touched. See [native-screen-b.md](native-screen-b.md).

## Why not MPEG-TS over MOST (the Škoda/MU1440 path)?

Static reading predicted it ([mu0678/static-map.md](../mu0678/static-map.md) §4) and the runtime result confirmed it:

- the config does contain a MOST encoder on `/dev/mlb/isoTX2`, but the `extended.2_lvds` profile adds `Tegra:HDMI0` as display
  4 and both HBAS profiles force `force_kombi_type = lvds`;
- the B9 Virtual Cockpit takes the **large navigation map over LVDS**; MOST carries menus, covers and cluster software.

So no `direct-ts-remux` / `isoTX2` writer is used here, and the `writev` gate approach does not apply.

## The compass window and the road-name bar

When CarPlay is the active navigation the cluster ECU switches to its own "navigation on mobile device" screen
(info state 6, a compass). That is a BAP state sent by the HMI, not a `dmdt` context. The road-name bar is also ECU-drawn
from BAP map data. Details: [TRACE-019](../traces/TRACE-019-road-name-bar.md), [open-issues.md](open-issues.md).
