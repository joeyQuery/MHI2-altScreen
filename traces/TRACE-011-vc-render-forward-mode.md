# TRACE-011 — vc_render and forward mode

**Question.** How can we avoid the `NvSSVideoClose` hang inside `dio_manager`?

**What we did.** Moved rendering to a separate `vc_render` process fed over 127.0.0.1:19830.

**Evidence.**

- [`011-r10-forward-mode.txt`](../evidence/logs/011-r10-forward-mode.txt)
- [`011-vc-render-first-run.txt`](../evidence/logs/011-vc-render-first-run.txt)
- [`011-vc-render-stable.txt`](../evidence/logs/011-vc-render-stable.txt)
- [`011-vc-render-relaunch.txt`](../evidence/logs/011-vc-render-relaunch.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`011-r10-forward-mode.txt`:
```
2018.694 [altscreen111] NATIVE-B FORWARD MODE ON: frames go to vc_render on 127.0.0.1:19830 (no NvSS in dio_manager)
```

`011-vc-render-first-run.txt`:
```
root@mmx:/mnt/app/root> cat /tmp/vc_render.log
```

**Finding.** Forward mode works: `dio_manager` only forwards bytes; `vc_render` owns NvSS and survives CarPlay restarts.

**Limits.** Relaunch behaviour depends on the autostart logic in TRACE-014.

**Related docs.** [architecture.md](../docs/architecture.md), [runtime-lifecycle.md](../docs/runtime-lifecycle.md)
