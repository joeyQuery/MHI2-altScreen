# TRACE-013 — View areas and turn switching

**Question.** How does the phone switch between two view areas on `turns`?

**What we did.** Logged `MODES_CHANGED` and `updateViewArea` intents.

**Evidence.**

- [`013-modes-changed-turns.txt`](../evidence/logs/013-modes-changed-turns.txt)
- [`013-viewarea-intents.txt`](../evidence/logs/013-viewarea-intents.txt)
- [`014-r25-r35-spawn-updateviewarea.txt`](../evidence/logs/014-r25-r35-spawn-updateviewarea.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`013-modes-changed-turns.txt`:
```
257.311 [altscreen111] GEN2 FLIGHT MODES_CHANGED parsed rc=0 seq=11 delta=0x00 screen=1 mainAudio=1 speech=0 speechDetail=-1 phone=0 turns=0 projection=1 source_aus=0 source_idrs=0 private111=1 policy=OBSERVE_ONLY
```

`013-viewarea-intents.txt`:
```
20.158 [altscreen111] GEN2 FLIGHT PlatformControl exit session=2e8458 command=startSession commandRef=783a3188 rc=0 out=0
```

`014-r25-r35-spawn-updateviewarea.txt`:
```
45.515 [altscreen111] GEN2 updateViewArea adjacentViewAreas mode=1 count=1
```

**Finding.** The phone applies the switch only when `updateViewArea` carries `adjacentViewAreas`.

**Limits.** Verified with one phone and one iOS version.

**Related docs.** [carplay-altscreen.md](../docs/carplay-altscreen.md), [runtime-lifecycle.md](../docs/runtime-lifecycle.md)
