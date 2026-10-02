# Safe areas — final values and measured rules

Files live in `/mnt/app/root`, format `x=`, `y=`, `w=`, `h=` one per line (all four required). Write with `mount -uw /mnt/app`, `echo`,
`mount -ur /mnt/app`; **replug the phone to reload** (they are read when CarPlay connects). Coordinates are in the 1440×542 declared canvas.

## Final values (owner-confirmed 2026-10-02)

| Layout | File | x | y | w | h |
| --- | --- | ---: | ---: | ---: | ---: |
| Small dials, idle | `mibr-carplay111-safearea-idle.conf` | 360 | 65 | 720 | 367 |
| Small dials, route | `mibr-carplay111-safearea.conf` | 468 | 85 | 840 | 332 |
| Big dials, idle | `mibr-carplay111-safearea-narrow-idle.conf` | 550 | 87 | 340 | 330 |
| Big dials, route | `mibr-carplay111-safearea-narrow-route.conf` | 550 | 87 | 340 | 330 |
| Third layout (untested) | `…-full-idle.conf`, `…-full-route.conf` | 114 | 88 | 350 | 296 |

Result: arrow centred in all four, the same position between idle and route (no visible jump), turn card positioned correctly, Google Maps
logo fine. The small-dials arrow was lowered about 40 px to match the big dials. Backups on the unit: `sa-backup` (before any change), `sa-backup2`,
`sa-backup3` (+ `safearea.conf.pre-center`).

## Rules learnt [runtime]

**Idle view**
- Arrow horizontal = `x + w/2`; arrow vertical = `y + 0.6·h` (it is not the area's vertical centre).
- The Google Maps logo follows the area's **bottom-left** corner (bottom edge `y + h` sets its height). The cluster gauges cover roughly stream
  x < 340 and x > 1100 below y ≈ 300 (inferred from angled photos), so `x ≥ 345` clears the left gauge.

**Route view (small dials)**
- Arrow horizontal ≈ `x + 0.3·w` (485 + 0.3·840 = 737 → `x = 468` gives ≈720).
- Vertically a **taller route area moves the arrow *up***, not down: `h = 322` left it low, 334 about 1 px high, 336 and 340 worse, **332 exact**
  (≈0.5 px of arrow per 1 px of `h`). Idle and route therefore do not follow the same vertical rule.
- The turn card follows the area's top edge `y` (lowering `y` raises the card) and its width; keep `y = 85` on the small dials. Changing only `h`
  does not move the card. Changing `y` and `h` together can move it the opposite way.
- Right edge: `x + w = 1380` clipped the card against the cluster's curved edge; 1325–1350 is clear.
- **"No space":** with route `w = 500` or 700 the Apple Maps card stretched over the whole screen and Waze showed none; 840 and 900 are fine.

**Big dials**
- The 0.3 horizontal rule did **not** predict this view; use a small window centred on 720 (`w = 340`).
- Card size follows the area width (460 → 400 → 340 shrank it); height 369 → 330 made no visible difference.
- Vertical position needs a large change in `y` (25 → 45 with the bottom fixed changed nothing visible; `y = 87` put the card in the right place).
- "No space" appears when the area is too short: `h ≤ 307` failed at `w = 460` and `h = 297` at `w = 400`; `h ≥ 330` worked.

| x | y | w | h | Result (big dials) |
| ---: | ---: | ---: | ---: | --- |
| 490 | 25 | 460 | 367 | card clipped at top, too big |
| 490 | 45 | 460 | 347 / 367 | same clip |
| 490 | 85 | 460 | 307 | "no space" |
| 520 | 87 | 400 | 297 | "no space" (the friend's value; his canvas is 540 high) |
| 520 | 87 | 400 | 369 | position perfect, card too big |
| 520 | 87 | 400 | 330 | size unchanged to the eye |
| **550** | **87** | **340** | **330** | **accepted** |

## Loader gotchas [static + runtime]

- `load_safearea_path` needs all four keys; a missing one logs `SafeArea config invalid` and the file is ignored.
- Values outside the **1440×456 startup canvas** are rejected (`SafeArea config out-of-range … using full canvas`) — `y + h` must stay ≤ 456 until the
  prepared r21 build (reads the size file earlier) is installed.
- **A rejected idle file silently turns two-view mode off** (the route config is then applied to the single view). After a replug check for
  `GEN2 two-view mode: idle=…`.
- A rejected slot-1 file means the slot is "not configured, unchanged" and the small-dials view stays.

## Earlier configuration (r20) [runtime]

Before the layout work, the two-view set was idle `345/25/810/367` (view 0) and route `485/108/840/255` (view 1), later route `y = 85` after the card clipped.
Those values are superseded by the table above; the history is in [TRACE-016](../traces/TRACE-016-safe-area-tuning.md).
