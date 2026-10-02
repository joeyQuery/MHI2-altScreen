# TRACE-015 — Cluster controls: zoom, wheel, layout detection

**Question.** Can the steering-wheel inputs and dial layout be read and used?

**What we did.** Used the HMI DSI tracer, `sloginfo` extracts and `setZoomArea` logging.

**Evidence.**

- [`015-tracer-extract.txt`](../evidence/logs/015-tracer-extract.txt)
- [`015-zoom-extract.txt`](../evidence/logs/015-zoom-extract.txt)
- [`015-layout-detection.txt`](../evidence/logs/015-layout-detection.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`015-layout-detection.txt`:
```
508.411 [altscreen111] r55 zone: setZoomArea Rect=370:49:700:300
```

`015-zoom-extract.txt`:
```
Jan 01 00:01:04    5 10000     0 00:00:26.486 hmi BENCH_Startup 2 31632 DSIAdapter.registerServiceProvider(org.dsi.ifc.map.DSIMapViewerZoomEngine,1)
```

**Finding.** Layout slots follow the `setZoomArea` Rect widths (700 = slot 0, 460 = slot 1). `changeMapZoomLevel` works.

**Limits.** Third layout never exercised.

**Related docs.** [cluster-controls.md](../docs/cluster-controls.md)
