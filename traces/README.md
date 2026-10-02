# Traces

Each runtime trace is a question, what we did, the evidence files, verbatim excerpts (checked by script against the files) and the finding. Raw (redacted) logs are in [`../evidence/logs/`](../evidence/logs/); see the [evidence register](../docs/evidence.md).

| # | Trace | Question |
|---|---|---|
| 001 | [Gen-1 hook baseline: what the unit advertises](TRACE-001-gen1-hook-baseline.md) | Does the hooked unit advertise a second (type 111) screen with `altScreen` and `viewAreas`, and with what geometry? |
| 002 | [r1 flight recorder: toggles and feature policy](TRACE-002-r1-flight-recorder.md) | Which `/info` choices change the phone's behaviour? Can we flip them without rebuilding? |
| 003 | [r3: HU info dump, hook install and size override](TRACE-003-r3-info-dump-and-clone.md) | Does the hook install cleanly on the MU0678 `dio_manager`, and can we override the advertised size at runtime? |
| 004 | [Stream 111 accepted, then silence on 6031](TRACE-004-stream111-6031-timeout.md) | The phone accepts the stream-111 SETUP. Why does it never connect to our data port? |
| 005 | [iPhone-side negotiation](TRACE-005-iphone-side-negotiation.md) | From the phone's own log: are the feature keys negotiated, is the cluster scene activated, and are two screen streams opened? |
| 006 | [The unit packet filter](TRACE-006-unit-packet-filter.md) | Why did the phone never reach port 6031? |
| 007 | [Native Screen B reaches H.264 decode](TRACE-007-native-screen-b.md) | Can the stock `dio_manager` receive stream 111 if we give it a stub video sink? |
| 008 | [First video on the cluster](TRACE-008-first-cluster-video.md) | Can the decoded frames be displayed on the cluster? |
| 009 | [Display-manager contexts and displayables](TRACE-009-display-manager-state.md) | Which `dmdt` context routes layer 58 to the cluster? |
| 010 | [Stock stream A and Screen B ordering](TRACE-010-stock-a-b-ordering.md) | Why did Screen B sometimes break stock CarPlay, and what order is safe? |
| 011 | [vc_render and forward mode](TRACE-011-vc-render-forward-mode.md) | How can we avoid the `NvSSVideoClose` hang inside `dio_manager`? |
| 012 | [Frame-rate ceiling and `features=2`](TRACE-012-framerate-features2.md) | Why was the stream capped near 16 fps? |
| 013 | [View areas and turn switching](TRACE-013-viewareas-turns.md) | How does the phone switch between two view areas on `turns`? |
| 014 | [Autostart, plug order, unplug and replug](TRACE-014-autostart-persistence.md) | Does it come up on its own, in any plug order, and recover after unplug? |
| 015 | [Cluster controls: zoom, wheel, layout detection](TRACE-015-cluster-controls.md) | Can the steering-wheel inputs and dial layout be read and used? |
| 016 | [Safe-area tuning](TRACE-016-safe-area-tuning.md) | What are the visible rectangles for each dial layout? |
| 017 | [`dio_manager` missing: triage](TRACE-017-dio-manager-triage.md) | `dio_manager` was not running once. Was it the hook? |
| 018 | [HUD / route-guidance experiment](TRACE-018-hud-route-guidance.md) | Can CarPlay route guidance reach the HUD with the stock cluster arrow suppressed? |
| 019 | [Road-name bar](TRACE-019-road-name-bar.md) | Can the native road-name bar be removed? |

Earlier static-analysis traces (before anything ran on the unit) are in [`static-phase/`](static-phase/README.md).
