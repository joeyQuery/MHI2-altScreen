# TRACE-001 — Gen-1 hook baseline: what the unit advertises

**Question.** Does the hooked unit advertise a second (type 111) screen with `altScreen` and `viewAreas`, and with what geometry?

**What we did.** Installed the Gen-1 hook (a `LD_PRELOAD` shim in front of `dio_manager`) and read its `/info` and candidate lines from `/tmp/altscreen111.log`.

**Evidence.**

- [`001-gen1-hook-baseline.txt`](../evidence/logs/001-gen1-hook-baseline.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`001-gen1-hook-baseline.txt`:
```
106.207 [altscreen111] GEN2 /info ready: root=altScreen+viewAreas type=111 maxFPS=30 features=0 input=none geometry=1440x455 physical=200x63 uuid=b7e6c5a0-2222-4000-8000-000000000002 url=maps:/car/instrumentcluster
```

`001-gen1-hook-baseline.txt`:
```
104.226 [altscreen111] GEN2 candidate active: 111=1440x455@30 physical=200x63 altPort=6031 tee=19820 URL=maps:/car/instrumentcluster uuid=b7e6c5a0-2222-4000-8000-000000000002 viewAreas=1 autoShow=0 bit26Mode=0
```

**Finding.** The unit-side `/info` root carries `altScreen+viewAreas`, type 111, 1440x455 at up to 30 fps, `features=0` and no input. These are the values everything later builds on.

**Limits.** This is what we advertise, not proof the phone accepts it (that is TRACE-005). Geometry was later changed (456 rows, then per-layout safe areas).

**Related docs.** [carplay-altscreen.md](../docs/carplay-altscreen.md), [architecture.md](../docs/architecture.md)
