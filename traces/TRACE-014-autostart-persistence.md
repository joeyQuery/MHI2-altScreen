# TRACE-014 — Autostart, plug order, unplug and replug

**Question.** Does it come up on its own, in any plug order, and recover after unplug?

**What we did.** Cold boots, early and late plugs, unplug, replug.

**Evidence.**

- [`014-coldboot-startup.txt`](../evidence/logs/014-coldboot-startup.txt)
- [`014-coldboot-extract.txt`](../evidence/logs/014-coldboot-extract.txt)
- [`014-r24-r25-autostart.txt`](../evidence/logs/014-r24-r25-autostart.txt)
- [`014-r25-r35-spawn-updateviewarea.txt`](../evidence/logs/014-r25-r35-spawn-updateviewarea.txt)
- [`014-unplug-and-forward-extract.txt`](../evidence/logs/014-unplug-and-forward-extract.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`014-r24-r25-autostart.txt`:
```
1:18.386 [altscreen111] r24-persist sync done
```

`014-r25-r35-spawn-updateviewarea.txt`:
```
root@mmx:/mnt/app/root> sed -n '/r25-autostart: spawn/,$p' /tmp/altscreen111.log | cut -c1-170 | head -45
```

**Finding.** Autostart and unplug → native map → replug work (builds r24–r44).

**Limits.** Timings are from one car.

**Related docs.** [runtime-lifecycle.md](../docs/runtime-lifecycle.md), [builds.md](../docs/builds.md)
