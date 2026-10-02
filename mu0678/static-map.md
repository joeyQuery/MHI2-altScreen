# Static map — written before anything ran on the car

Method: offline analysis of a head-unit dump plus the public upstream repo's audited Škoda MU1440 / Audi MU1438 research. This was the plan, **not** the result; the runtime outcome is in [STATUS.md](../STATUS.md). Tags: **[PROVEN-STATIC]**, **[INFERRED]**, **[UNKNOWN]**. Identity and hashes: [firmware.md](firmware.md).

## Bottom line (as of 2026-09-29, before runtime)

| Layer | Status then | Outcome later |
| --- | --- | --- |
| Stream-111 receive (hook in `dio_manager`) | strong candidate | **works** ([TRACE-001](../traces/TRACE-001-gen1-hook-baseline.md)) |
| `smartphone_integrator.json` preload | same mechanism, different layout (multi-line `envs`) | handled by [`tools/make_patched_carplay_json.py`](../tools/make_patched_carplay_json.py) |
| H.264 → MPEG-TS remux | capture/analysis tool only | not the display path |
| MOST `/dev/mlb/isoTX2` writer | wrong sink for this LVDS-fed cluster | confirmed unused |
| Cluster presentation | new work, needed runtime data | solved via NvSS output 1 / layer 58 ([TRACE-008](../traces/TRACE-008-first-cluster-video.md)) |

"MHI2 units are near identical" is true for the CarPlay receiver side and false for the display side.

## Loading mechanism [PROVEN-STATIC]

`smartphone_integrator` starts CarPlay as `dio_manager` using `children.carplay.envs` in `/mnt/system/etc/eso/production/smartphone_integrator.json`; the hook rides in via `LD_PRELOAD`. On MU0678 that array is multi-line, so upstream's awk patcher would corrupt it. The generator script refuses any input whose SHA-256 is not the known stock (`6e7968e0…`) and yields exactly 10 env entries (upstream's budget).

## Why MOST is not the path [PROVEN-STATIC config + upstream OEM evidence]

`displaymanager.json` contains a MOST encoder on `/dev/mlb/isoTX2` and a single production LVDS terminal (`Tegra:TFTLCD0`, id 0). The `extended.2_lvds` profile adds `Tegra:HDMI0` as display **id 4**, and both HBAS profiles force `force_kombi_type = lvds`. Upstream's `lsd.jxe` analysis maps `DISPLAYID_CLUSTER = 4`. Audi training material (cited by upstream) says the B9 large map goes over LVDS; MOST carries lists, covers and cluster software.

## Presenters considered [INFERRED then]

Displayables in contexts 72–78 on display 4: 33 `KOMBI_MAP_VIEW`, 58 `GOOGLE_EARTH_KOMBI_MAP_VIEW`, 20 `MAP_ROUTE_GUIDANCE`, 59 `EXTERNAL_SMARTPHONE`, 101/102 KDK backing images.

- **P-A** second NvSS decode → HDMI output (`NVSS_VIDEO_OUTPUT_DEVICE` 1 = HDMI0 = display 4). **This is what worked.**
- **P-B** put displayable 59 in a cluster context via Java/HMI override. Not pursued.
- **P-C** feed frames into displayable 33's texture. Not pursued.

## Gates

| Gate | Question | Result |
| --- | --- | --- |
| G1 | On-car hashes match the dump? | yes (displaymanager.json confirmed) |
| G2 | Production paths and remountable `/mnt/app`, `/mnt/system`? | yes ([deployment](../docs/deployment.md)) |
| G3 | Do the differing `dio_manager`/`smartphone_integrator` still let the hook install? | yes, via a MAP_FIXED clone fallback ([TRACE-003](../traces/TRACE-003-r3-info-dump-and-clone.md)) |
| G4 | Does iOS accept the type-111 SETUP and send H.264? | yes ([TRACE-005](../traces/TRACE-005-iphone-side-negotiation.md)) |
| G5 | Cluster raster | 1440×455 native map; coded 1440×456 |
| G6 | Presenter and context | P-A, context 77 ([TRACE-009](../traces/TRACE-009-display-manager-state.md)) |
| G7 | Stock → project → stock recovery | restore script in [`deployment/`](../deployment/) |

## Privacy

The dump contained EEPROM/FEC/serial data and personal media. None of it is copied into this repo. Do not commit a unit dump.
