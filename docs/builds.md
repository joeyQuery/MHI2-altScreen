# Build history (hook tags rNN)

The hook was patched in small steps; each step is a numbered build. The numbers appear in log lines (`r25-autostart: spawn`, `r56 layout …`, `r60 start layout …`).
The C source still carries the tag string `audi-mu0678-r23-rxstats`, so the log line `build audi-mu0678-…` shows r23 for every build after it; use the per-feature lines.

| Build | Change | Evidence |
| --- | --- | --- |
| r1 | first boot of the Audi hook; `no-alt-display` marker; features policy stock | [TRACE-002](../traces/TRACE-002-r1-flight-recorder.md) |
| r2–r3 | display clone stage 0/6, `/info` dump, size override 1010×376, features `replace` | [TRACE-003](../traces/TRACE-003-r3-info-dump-and-clone.md), [TRACE-004](../traces/TRACE-004-stream111-6031-timeout.md) |
| r4 | receiver on port 6031 (pre-native) | [TRACE-004](../traces/TRACE-004-stream111-6031-timeout.md) |
| r5 | native Screen B, stub sink (`audi-mu0678-r5-nativeb-p1`) | [TRACE-007](../traces/TRACE-007-native-screen-b.md) |
| r6 | B's real NvSS, `outputDevice 1`, layer 58 (`r6-nativeb-disp`) | [TRACE-008](../traces/TRACE-008-first-cluster-video.md) |
| r7 | stream 111 on port 6000 (`r7-port6000`) | [TRACE-008](../traces/TRACE-008-first-cluster-video.md) |
| r9 | B waits for stock A (`r9-order`) | [TRACE-010](../traces/TRACE-010-stock-a-b-ordering.md) |
| r10 | forward mode to `vc_render` (`r10-fwd`) | [TRACE-011](../traces/TRACE-011-vc-render-forward-mode.md) |
| r16–r21 | two view areas, `updateViewArea` on `turns`; `/tmp` switches for animation, `viewAreaTransitionControl`, view-1 origin (inert); **r19/r20 `adjacentViewAreas`, default on**; r21 early size read (prepared, **not installed**) | [TRACE-013](../traces/TRACE-013-viewareas-turns.md) |
| r22 | `TCP_NODELAY` on the forward socket (no fps change) | [open-issues.md](open-issues.md) |
| r23 | hook-side `NATIVE-B rx … fps` counter | [open-issues.md](open-issues.md) |
| r24 | persistent markers, stage 7 default, `r24-persist sync done` | [TRACE-014](../traces/TRACE-014-autostart-persistence.md) |
| r25 | hook starts `vc_render` and `dmdt sc 4 77` | [TRACE-014](../traces/TRACE-014-autostart-persistence.md) |
| r26 | fd-map spawn (did not build on this toolchain) | — |
| r27 | `FD_CLOEXEC` around `spawnve()` | [runtime-lifecycle.md](runtime-lifecycle.md) |
| r29–r33 | wait for the second `NavStateOfOperation to 5`, then re-acquire | [runtime-lifecycle.md](runtime-lifecycle.md) |
| r34 | no waiting; renderer at once, switch every 3 s | [runtime-lifecycle.md](runtime-lifecycle.md) |
| r35, r38, r39, r41 | **reverted/failed** (spawn or marker from the teardown path; fast path without the gate) | [disproven.md](disproven.md) |
| r36 | removed the cold-boot re-acquire — **reverted** | [disproven.md](disproven.md) |
| r40 / r44 | teardown watcher → `dmdt sc 4 72`; r44 = own spawned shell, once per hook load | [runtime-lifecycle.md](runtime-lifecycle.md) |
| r42 / r43 | phone-present gate `P()`; replug fast path | [runtime-lifecycle.md](runtime-lifecycle.md) |
| r45–r49 | clone-knob HID wiring (`HIDPostReport`) — not used | [carplay-altscreen.md](carplay-altscreen.md) |
| r50 | `features` override marker | — |
| r51 / r52 / r53 | zoom command, wheel reader, wheel-click detection (no action) | [TRACE-015](../traces/TRACE-015-cluster-controls.md) |
| r48 | faster cold boot (retry 250 ms, keyframe wait 500 ms, `NB_FWD_FK_MAX 3`) | [cluster-controls.md](cluster-controls.md) |
| r55 / r56 | layout detection from `setZoomArea`; per-layout views (`ALT111_MAX_VIEWS 6`) | [TRACE-015](../traces/TRACE-015-cluster-controls.md) |
| r57 | `komoview` logging | — |
| r58 / r59 | bluepirat reader (port 20991) for boot-time layout | [TRACE-015](../traces/TRACE-015-cluster-controls.md) |
| r60 | remember the last layout across `dio_manager` restarts | [runtime-lifecycle.md](runtime-lifecycle.md) |

Build: `bash build/build_gen2_audi.sh` from the repository root (needs the QNX 6.5 ARMv7 toolchain; set `QNX_RUN`). The patches were applied to the source in
small Python edits on the workstation and are folded into the published source; the intermediate apply scripts are not published.

**Reconciliation note:** the source in this tree is the workstation source at r60. An earlier working copy used in one session carried an `r20-adjdefault` tag with the
same default-1 adjacency change; it was reconciled by taking the workstation source.
