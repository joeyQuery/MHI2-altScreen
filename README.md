# MHI2 AltScreen — Audi MU0678 / B9 Virtual Cockpit

**Open research and engineering for CarPlay secondary-screen navigation on the Audi Virtual Cockpit, on an Audi MHI2 head unit (MU0678, A4 B9).**

> [!IMPORTANT]
> ## ✅ WORKING ON THE REFERENCE VEHICLE — AUDI MU0678 + VIRTUAL COCKPIT
>
> **Live CarPlay navigation from the phone's secondary screen (stream type 111) runs in the factory Audi
> Virtual Cockpit at about 29–30 frames per second**, with an automatic start after boot, a return to the
> native map when the phone is unplugged, steering-wheel zoom, and a map that follows the cluster's
> dial layout.

[**Status**](STATUS.md) · [Roadmap](ROADMAP.md) · [Docs index](docs/README.md) · [Traces](traces/README.md) · [Evidence register](docs/evidence.md) · [Open issues](docs/open-issues.md) · [What did not work](docs/disproven.md)

> [!WARNING]
> **Research / experimental / use at your own risk.** The hook runs inside `dio_manager`. Mistakes can leave the
> CarPlay stack, or in the worst case the infotainment, unusable until you restore it. Keep backups, understand
> every step, and never test while driving. Nothing here was tested on any other head unit or cluster.

> [!IMPORTANT]
> The only vehicle-proven target is **Audi A4 B9 (2016) · MHI2 `MU0678` · firmware train `P3241` ·
> Virtual Cockpit cluster part `8W5920790C`**. Another MHI2 train, another cluster, or another brand must be
> checked **for the head unit *and* the cluster** before anything here is assumed to apply. Same-looking
> units are **not** a compatibility claim — read [firmware identity and hashes](mu0678/firmware.md).

## Project goals

1. **CarPlay AltScreen map in the Virtual Cockpit** on MHI2 units whose cluster is fed over LVDS rather than MOST.
2. Understand the display path (`displaymanager`, contexts, NvSS layers) well enough to take it over **without
   breaking the stock fallback** (native map on unplug, native behaviour with no phone).
3. Make the **cluster controls** usable: steering-wheel zoom, the MMI's own dial-size button, menus.
4. Feed the **HUD** from CarPlay's turn-by-turn guidance, if the unit's iAP2 stack allows it.
5. Leave enough evidence — traces, logs, hashes, failed attempts — that someone else can continue without
   repeating the reverse engineering.

## Current target

| Item | Value |
| --- | --- |
| Vehicle | Audi A4 B9 (2016), right-hand drive, Virtual Cockpit |
| Head unit | MHI2 `MU0678`, build `MHI2_ER_AUG22_P3241`, project `mmx2_audi_er_g22`, framework 5.41.10.SR5 |
| Cluster | `8W5920790C`, native map area 1440×455 (declared canvas 1440×542) |
| CarPlay stack | HARMAN iAP2 (`mm-ipod`), `dio_manager` + `libairplay.so` |
| Phone used | iPhone (iOS 26.3.1), USB CarPlay |
| Access | SSH over a USB-LAN adapter / WLAN, QNX 6.5 ksh |

## Current state

What is **proven on the car** (every line links to the trace that shows it):

- stock CarPlay uses **stream 110** (MMI); the **second screen is stream type 111**, negotiated with the
  `altScreen` + `viewAreas` feature keys — [TRACE-001](traces/TRACE-001-gen1-hook-baseline.md), [TRACE-005](traces/TRACE-005-iphone-side-negotiation.md);
- the unit's own **packet filter silently dropped our 111 listener** (port 6031); stream 111 had to move to an
  allowed port (6000) — [TRACE-006](traces/TRACE-006-unit-packet-filter.md);
- a **native `Screen B`** (stock `libairplay` machinery, stub video sink) accepts stream 111 and reaches the
  H.264 decode boundary — [TRACE-007](traces/TRACE-007-native-screen-b.md);
- rewriting only B's `NvSSVideoOpen` (`outputDevice 0→1`, layer `59→58`) and switching display 4 to context 77
  puts the picture on the cluster — [TRACE-008](traces/TRACE-008-first-cluster-video.md), [TRACE-009](traces/TRACE-009-display-manager-state.md);
- a **separate `vc_render` process** must own the cluster's NvSS instance, otherwise stock `NvSSVideoClose` hangs
  inside `dio_manager` when you leave CarPlay — [TRACE-010](traces/TRACE-010-stock-a-b-ordering.md), [TRACE-011](traces/TRACE-011-vc-render-forward-mode.md);
- the **frame-rate limit was the phone throttling stream 111** (~16 fps) because our display record advertised
  `features=0`; `features=2` + a knob HID restored ~30 fps — [TRACE-012](traces/TRACE-012-framerate-features2.md);
- **two view areas** (idle / route) switched on the phone's `turns` field, honoured only when `updateViewArea`
  carries `adjacentViewAreas` — [TRACE-013](traces/TRACE-013-viewareas-turns.md);
- **autostart, early/late plug, unplug → native map, replug** — [TRACE-014](traces/TRACE-014-autostart-persistence.md);
- **scroll-wheel zoom** (`changeMapZoomLevel`, `zoomDirection` 0 in / 1 out) and **dial-layout detection**
  (`DSIMapViewerZoomEngine#setZoomArea` rectangles) — [TRACE-015](traces/TRACE-015-cluster-controls.md);
- **per-layout safe areas** for the small and big dials — [TRACE-016](traces/TRACE-016-safe-area-tuning.md).

What is **experimental or not finished**:

- **HUD next-turn** from CarPlay route guidance: the data path was observed working with a patched third-party
  hook; the package is currently **rolled back** and one unexplained CarPlay session drop at route end is open —
  [TRACE-018](traces/TRACE-018-hud-route-guidance.md);
- the **native road-name bar** cannot be removed from the stream side; a Java experiment failed —
  [TRACE-019](traces/TRACE-019-road-name-bar.md);
- menu to switch back to the native map, disabling the cluster's left/right menus (keys 99/100);
- the third dial layout (not available on the test car), Waze aliasing, a safe installer.

See [STATUS.md](STATUS.md) and [open issues](docs/open-issues.md).

## Architecture in one picture

```text
iPhone ── USB ──► mm-ipod / iAP2 ──► dio_manager (+ libaltscreen111 hook, LD_PRELOAD)
                                       │
          stream 110 (main) ───────────┼──► stock NvSS → MMI display (unchanged)
                                       │
          stream 111 (alt screen) ─────┼──► native "Screen B" (stub sink) ──► H.264 access units
                                       │                                          │ 127.0.0.1:19830
                                       │                                          ▼
                                       │                                    vc_render (own process)
                                       │                                          │ NvSSVideoOpen(outputDevice 1, layer 58)
                                       ▼                                          ▼
                              displaymanager ◄──────── `dmdt sc 4 77` ──── context 77 {20,102,101,58}
                                       │
                                       ▼
                       cluster display (2nd_lvds, 1440×542) ──► Virtual Cockpit
```

Two things run beside it: a **tracer client** that reads the HMI's key and layout events (zoom, dial size), and the
**safe-area / view-area logic** that tells the phone where the map arrow and turn card may sit.
Details: [architecture](docs/architecture.md), [display path](docs/display-path.md),
[native Screen B](docs/native-screen-b.md), [runtime lifecycle](docs/runtime-lifecycle.md).

## Start here

1. [Status](STATUS.md) — what works, what does not, what is unknown.
2. [Architecture](docs/architecture.md) — media plane vs control plane, and who owns the cluster surface.
3. [CarPlay AltScreen protocol notes](docs/carplay-altscreen.md) — stream 111, feature keys, `suggestUI`/`showUI`, view areas, zoom.
4. [Display path on this car](docs/display-path.md) — `displaymanager`, contexts, NvSS layers, why not MOST.
5. [Native Screen B and vc_render](docs/native-screen-b.md) — the hook design and the five things that had to be right.
6. [Runtime lifecycle](docs/runtime-lifecycle.md) — autostart, navigation-ready wait, unplug, replug.
7. [Cluster controls](docs/cluster-controls.md) — tracer, key IDs, zoom, dial layouts.
8. [Safe areas](docs/safe-areas.md) — final values and the measured rules.
9. [HUD route guidance](docs/hud-route-guidance.md) — what was tried and where it stands.
10. [What did not work](docs/disproven.md) — corrected interpretations and dead ends.
11. [Evidence register](docs/evidence.md) — every claim, its status, and the trace behind it.
12. [Trace index](traces/README.md) — 19 runtime traces plus the earlier static-analysis traces.
13. [Firmware identity and hashes](mu0678/firmware.md) · [static map](mu0678/static-map.md).
14. [Deployment and restore](docs/deployment.md) · [QNX shell notes](docs/ssh-environment.md) · [build history](docs/builds.md).

## Repository layout

```text
.
├── README.md  STATUS.md  ROADMAP.md  LICENSE  THIRD_PARTY_NOTICES.md
├── docs/                 architecture, protocol, display path, lifecycle, controls, safe areas, HUD, evidence
│   └── static-phase/     the earlier static-analysis documents (kept as research record)
├── traces/               TRACE-001…019 runtime traces; static-phase/ holds the earlier static traces
├── mu0678/               platform notes: firmware identity, hashes, static map
├── src/
│   ├── gen2-audi/        the hook (libaltscreen111): C sources, native_b.inc, headers
│   └── vc_render/        the standalone cluster renderer
├── build/                QNX cross-build scripts
├── deployment/           install / swap / restore scripts for the unit
├── hud/                  HUD route-guidance install & rollback (third-party binaries are NOT included)
├── tools/                evidence generators, config generator, jar patcher
├── artifacts/mu0678/     build outputs with SHA-256 and provenance
└── evidence/             raw (redacted) logs, configs, process lists, hash evidence
```

## Reproducibility and binaries

- **Project code** (hook, renderer, scripts) is published as source with a pinned build script. The binaries under
  `artifacts/` are built from exactly that source and listed with SHA-256 in
  [artifacts/mu0678/SHA256SUMS](artifacts/mu0678/SHA256SUMS).
- **Third-party material is not mirrored.** OEM firmware, Apple binaries and the upstream route-guidance binaries are
  referenced by hash only. The route-guidance jar patch is shipped as a **script** you run on your own copy
  ([tools/patch_rgd_hook_jar.py](tools/patch_rgd_hook_jar.py)).
- **Logs are redacted** (street names, MAC and IPv6 addresses, VIN/IMSI, account names, phone-side UUIDs).
  Everything else is verbatim; each file says what was changed. Photos are not published (they show a real location).

## Access model

You need a working shell on your own unit (SSH over WLAN or a USB-LAN adapter). This repository does not teach
or bundle access methods; those belong to the MIB community projects. See [SSH environment](docs/ssh-environment.md)
for the QNX shell constraints the scripts were written against.

## Testers wanted

- another **Audi MHI2 / Virtual Cockpit** unit (especially a different firmware train or the 12.3″ cluster);
- the **third dial layout** (the test car only has two);
- anyone who can explain the **CarPlay session drop at route end** seen with the HUD hook;
- QNX / `displaymanager` / BAP experience (the road-name bar and the cluster menus).

Please use Discussions first for a new platform; open an Issue for a reproducible bug with the log tag
(`build audi-mu0678-rNN`) and the safe-area files you used.

## Prior art and credits

This work stands on public MIB/MQB community research. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
In particular: the **MHI2 AltScreen CarPlay** project (`harman-f/mhi2_altscreen_carplay`, GPL-3.0-or-later) whose GEN2
hook this code was ported from; **OneB1t / VcMOSTRenderMqb**; **chefranov/mhi2-au37x-carplay** (route-guidance
hook, studied and patched, not redistributed); **Luka's** CarPlay-RGI work; and the wider **M.I.B.** community.

## Independence / trademarks

An independent interoperability and reverse-engineering project. It is **not affiliated with, sponsored by, or
endorsed by** Harman, Apple, Audi AG, Volkswagen AG or their affiliates. Product names belong to their owners.

## License

Original project code and documentation: **GNU General Public License v3.0 or later**
(`SPDX-License-Identifier: GPL-3.0-or-later`) — see [LICENSE](LICENSE). Third-party material keeps its own terms.

> **Trace it. Prove it. Document it.**
