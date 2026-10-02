# HUD route guidance — paused, rolled back

> **Not working end to end.** The data path was observed, but the package was rolled back on the unit; see [docs/hud-route-guidance.md](../docs/hud-route-guidance.md) and [TRACE-018](../traces/TRACE-018-hud-route-guidance.md).

- [`hud_install.sh`](hud_install.sh) expects a `bin/` directory next to it containing `rgd_hook.jar` (patched, v4) and `librgd_hook.so`. Those are **not** in this repo; see [UPSTREAM.md](UPSTREAM.md).
- [`hud_rollback.sh`](hud_rollback.sh) removes the hook and restores the stock files; restart the car after it. After one rollback CarPlay needed an MMI restart to come back — expect that.
- Open questions: does the session drop at route end also happen with route guidance disabled; what draws the arrow on the small dials; the `NoSuchMethodError` in the shutdown sync.
