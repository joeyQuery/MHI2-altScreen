# Upstream for the HUD experiment

The route-guidance work builds on **chefranov/mhi2-au37x-carplay**, commit `8968119c136b1f148486149de0950058829a2f33`. That repository carries **no licence**, so its binaries are **not redistributed** here. This repo ships only a patch script; fetch the upstream files yourself.

| Upstream file | SHA-256 |
| --- | --- |
| `bin/rgd_hook.jar` | `e53cb9081e0a2cb617771a26c606b3977bd0496d98eaad36df0302d34f7bc921` |
| `bin/librgd_hook.so` | `2661df624b7ae6eb03e1c103726fe59f99dda88b33bd6c5166747057405f4b52` |
| `bin/rgd_blank.png` | (fetch upstream) |

Patch: `python3 tools/patch_rgd_hook_jar.py rgd_hook.jar rgd_hook.hud.jar`. Three byte patches: `ctxEnabled` → false; `pushClusterManeuver` → return; `DEFERRED_SHUTDOWN_MS` 6000 → 3000. Class contents match the jar we tested on the unit (zip-level SHA-256 differs only by metadata; the tested jar was `84fe27f476e387d8a2c5c99fce67d9fd61f04e57013079d84c25a422891d871a`).
