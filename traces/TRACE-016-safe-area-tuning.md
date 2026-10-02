# TRACE-016 — Safe-area tuning

**Question.** What are the visible rectangles for each dial layout?

**What we did.** Iterated safe-area files, logging each `r55/r56` zone.

**Evidence.**

- [`016-safearea-files.txt`](../evidence/logs/016-safearea-files.txt)
- [`016-r23-restart-persist.txt`](../evidence/logs/016-r23-restart-persist.txt)
- [`016-r23-two-view-sed-noise.txt`](../evidence/logs/016-r23-two-view-sed-noise.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`016-safearea-files.txt`:
```
508.411 [altscreen111] r55 zone: setZoomArea Rect=370:49:700:300
```

`016-r23-restart-persist.txt`:
```
595.654 [altscreen111] renderer tee listening on 127.0.0.1:19820
```

**Finding.** Final rectangles are in `docs/safe-areas.md`.

**Limits.** The sed-noise file is a failed command kept for completeness.

**Related docs.** [safe-areas.md](../docs/safe-areas.md)
