# TRACE-010 — Stock stream A and Screen B ordering

**Question.** Why did Screen B sometimes break stock CarPlay, and what order is safe?

**What we did.** Compared runs where B started before and after stock A configured; looked at stopped `dio_manager` threads.

**Evidence.**

- [`010-r7-stock-a-b-race.txt`](../evidence/logs/010-r7-stock-a-b-race.txt)
- [`010-r9-order.txt`](../evidence/logs/010-r9-order.txt)
- [`010-dio-manager-stopped-threads.txt`](../evidence/logs/010-dio-manager-stopped-threads.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`010-r7-stock-a-b-race.txt`:
```
262.002 [altscreen111] NATIVE-B DISPLAY MODE ON: outputDevice=1 layerID=58 size=1440x456 offset=0,0
```

`010-r9-order.txt`:
```
67.394 [altscreen111] NATIVE-B DISPLAY MODE ON: outputDevice=1 layerID=58 size=1440x456 offset=0,0
```

`010-dio-manager-stopped-threads.txt`:
```
 4378757   1 n/apps/dio_manager  10r CONDVAR     (0xffbe0)
```

**Finding.** B must be started after A has configured. Tearing down 110 must not take 111 down. Threads blocked on a condvar showed the close hang.

**Limits.** Ordering was established empirically, not from source.

**Related docs.** [runtime-lifecycle.md](../docs/runtime-lifecycle.md), [native-screen-b.md](../docs/native-screen-b.md)
