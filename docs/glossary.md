# Glossary

| Term | Meaning |
| --- | --- |
| MHI2 / MIB2 | VW-group head-unit family (Harman hardware, QNX 6.5, ARMv7). |
| Stream 110 / 111 | Stock CarPlay main screen is stream 110. Stream 111 is the second `altScreen` stream carrying the cluster map. |
| `altScreen`, `viewAreas` | Feature keys negotiated in the CarPlay `/info` root; `adjacentViewAreas` is needed in `updateViewArea` for switching areas. |
| `dio_manager` | The process that hosts the CarPlay/AirPlay receiver on the unit. |
| Screen B | The native second-screen object in `dio_manager` that receives stream 111. |
| NvSS | Nvidia video decode/output service; `outputDevice` 0 = LVDS, 1 = HDMI0. |
| `vc_render` | Our separate process that owns NvSS output for the cluster, fed over 127.0.0.1:19830. |
| `dmdt` | DisplayManager diagnostic tool; `dmdt sc 4 77` sets display 4 to context 77. |
| Displayable / context | DisplayManager objects: contexts are lists of displayables placed on a display. |
| Display 4 | `2nd_lvds` / `Tegra:HDMI0`, the Virtual Cockpit. |
| `pf` | The unit's packet filter; limits inbound ports on `carplay0`. |
| BAP | VW cluster bus protocol; carries the native navigation maneuver data. |
| HUD | Head-up display; fed through the route-guidance path (see [hud-route-guidance.md](hud-route-guidance.md)). |
| `setZoomArea` | `DSIMapViewerZoomEngine` call whose Rect reveals the cluster dial layout. |
