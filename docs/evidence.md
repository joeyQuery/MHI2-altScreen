# Evidence register

Every claim in [STATUS.md](../STATUS.md), its status and where the proof is. **Verified** = seen in a log on the car or phone. **Observed** = seen by eye on the cluster (no file). **Open** = not established.

| Claim | Status | Trace | Raw log(s) |
| --- | --- | --- | --- |
| Hook advertises type 111 with `altScreen+viewAreas` | Verified | [001](../traces/TRACE-001-gen1-hook-baseline.md) | `001-*` |
| Phone negotiates keys, shows cluster scene, opens two screen streams | Verified | [005](../traces/TRACE-005-iphone-side-negotiation.md) | `005-*` |
| pf drops all but 6 inbound port ranges on `carplay0` | Verified | [006](../traces/TRACE-006-unit-packet-filter.md) | `006-*` |
| Stream 111 accepted on port 6000 | Verified | [007](../traces/TRACE-007-native-screen-b.md) | `007-*` |
| Context 77 on display 4 shows layer 58 | Verified (table) + Observed (picture) | [008](../traces/TRACE-008-first-cluster-video.md), [009](../traces/TRACE-009-display-manager-state.md) | `008-*`, `009-*` |
| Start order A before B; close hang in `dio_manager` | Verified | [010](../traces/TRACE-010-stock-a-b-ordering.md) | `010-*` |
| Forward mode to `vc_render` | Verified | [011](../traces/TRACE-011-vc-render-forward-mode.md) | `011-*` |
| `features=2` lifts the fps cap | Observed + log of record | [012](../traces/TRACE-012-framerate-features2.md) | `012-*` |
| Turn switching needs `adjacentViewAreas` | Verified | [013](../traces/TRACE-013-viewareas-turns.md) | `013-*`, `014-r25-*` |
| Autostart, unplug, replug | Verified | [014](../traces/TRACE-014-autostart-persistence.md) | `014-*` |
| Zoom and layout slots | Verified | [015](../traces/TRACE-015-cluster-controls.md) | `015-*` |
| Safe areas | Observed + files | [016](../traces/TRACE-016-safe-area-tuning.md) | `016-*` |
| `dio_manager` missing once | Open | [017](../traces/TRACE-017-dio-manager-triage.md) | `017-*` |
| HUD feed, session drop, small-dial arrow | Open | [018](../traces/TRACE-018-hud-route-guidance.md) | `018-*` |
| Road-name bar removal | Open (negative result, no raw log) | [019](../traces/TRACE-019-road-name-bar.md) | — |

## Redaction

Logs under [`../evidence/logs/`](../evidence/logs/) are redacted: street names, MAC and link-local addresses, account/host names, team IDs, VIN/IMSI and phone-side UUIDs. Lines were not otherwise edited. Excerpts in each trace are checked against these files by script.

## Not included, on purpose

OEM binaries (`lsd.jxe`, `displaymanager`, the graphics ELF), the raw H.264 capture, the OEM-derived cluster-bar jar, slog dumps, other `smartphone_integrator.json` variants, third-party binaries, and photographs (they show a home location). Hashes of the shipped artifacts are in [`../artifacts/mu0678/SHA256SUMS`](../artifacts/mu0678/SHA256SUMS).
