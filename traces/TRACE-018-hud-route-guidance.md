# TRACE-018 — HUD / route-guidance experiment

**Question.** Can CarPlay route guidance reach the HUD with the stock cluster arrow suppressed?

**What we did.** Used a patched third-party hook jar (see `hud/UPSTREAM.md`).

**Evidence.**

- [`018-hud-carplay-hook-log.txt`](../evidence/logs/018-hud-carplay-hook-log.txt)
- [`018-hud-jar-size-and-pool.txt`](../evidence/logs/018-hud-jar-size-and-pool.txt)
- [`018-hud-shutdown-cycles.txt`](../evidence/logs/018-hud-shutdown-cycles.txt)
- [`018-hud-takeover-and-r56.txt`](../evidence/logs/018-hud-takeover-and-r56.txt)
- [`018-hud-pastes.txt`](../evidence/logs/018-hud-pastes.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`018-hud-shutdown-cycles.txt`:
```
118690 [INF] [BAPBridge] cluster-state[shutdown] clusterService=null | sync type=0 state=1 pending=false cancelled= | sync threw java.lang.NoSuchMethodError: de/audi/app/bap/fw/functionsync/IFunctionS
```

`018-hud-carplay-hook-log.txt`:
```
root@mmx:/mnt/app/root> grep -i -e suppress -e Started -e "RG presentation" -e "cluster" -e BAP /tmp/carplay_hook.log | tail -50 | cut -c1-220
```

**Finding.** Data path observed; session dropped at route end; package rolled back.

**Limits.** Paused; unexplained items in `docs/hud-route-guidance.md`.

**Related docs.** [hud-route-guidance.md](../docs/hud-route-guidance.md), [open-issues.md](../docs/open-issues.md)
