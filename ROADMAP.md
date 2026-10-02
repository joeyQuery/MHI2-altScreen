# Roadmap

Priorities, in the order the owner set them. Each item lists what is known and what blocks it.

## Done (on the reference car)

- Stream 111 → native Screen B → `vc_render` → cluster, ~30 fps.
- Autostart, early/late plug, unplug → native map, replug.
- Scroll-wheel zoom; dial-layout detection; per-layout safe areas (two layouts).

## Next

1. **HUD next turn.** Redo the experiment from a cold start with the hook off (`rgd_disable`) first, to isolate the
   session drop at route end. Decide what to do about the cluster arrow on the small dials.
2. **Menu / action to return to the native map.** `dmdt sc 4 72` already does it from the hook's watcher; the open
   part is the trigger (the wheel click is deliberately unmapped).
3. **Disable the cluster's left (key 99) and right (key 100) menus** while the altscreen is showing.
4. **Issue 1 — the native road-name bar.** Parked until last. Lead: the bar changes size with the dial layout, so the
   cluster knows the layout; BAP map-presentation flags are the candidate lever.

## Housekeeping

- Revert the DSITracer configuration when it is no longer needed (restore `traceConfig.properties.orig`, remove
  `dsitracer_activated`).
- Install the prepared early-size build (r21) and drop the inert r17/r18 switches.
- Capture raw logs for the zoom, tracer and unplug tests (the findings are recorded; the raw pastes were not archived).
- A guarded installer with hash gates for exactly this firmware train.

## Help wanted

See the README: other MHI2 units, the third dial layout, QNX / BAP / `displaymanager` experience.
