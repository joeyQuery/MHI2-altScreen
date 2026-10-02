# TRACE-008 — First video on the cluster

**Question.** Can the decoded frames be displayed on the cluster?

**What we did.** Rewrote `NvSSVideoOpen` to `outputDevice 1`, layer 58, then switched the cluster context with `dmdt`.

**Evidence.**

- [`008-r7-first-vc-video.txt`](../evidence/logs/008-r7-first-vc-video.txt)
- [`008-r7-session-log.txt`](../evidence/logs/008-r7-session-log.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`008-r7-first-vc-video.txt`:
```
1034.969 [altscreen111] NATIVE-B DISPLAY MODE ON: outputDevice=1 layerID=58 size=1440x456 offset=0,0
```

`008-r7-session-log.txt`:
```
341.915 [altscreen111] cannot bind renderer tee port 19820: Address already in use
```

**Finding.** Picture on the cluster, via output device 1 and layer 58. The same logs show a second-instance collision on the renderer tee port (`Address already in use`), which is why later builds own that port from a single process.

**Limits.** The excerpts shown are the stub registration and the port collision; the outputDevice/layer rewrite lines themselves are not in these redacted files. This first version ran NvSS inside `dio_manager`, which later hung on close (TRACE-010/011).

**Related docs.** [display-path.md](../docs/display-path.md), [native-screen-b.md](../docs/native-screen-b.md)
