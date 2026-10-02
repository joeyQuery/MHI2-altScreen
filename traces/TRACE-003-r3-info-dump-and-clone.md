# TRACE-003 — r3: HU info dump, hook install and size override

**Question.** Does the hook install cleanly on the MU0678 `dio_manager`, and can we override the advertised size at runtime?

**What we did.** Ran r3 with an info dump and a size-override file; watched the `SessionSetup` hook patching.

**Evidence.**

- [`003-r3-hu-info-dump.txt`](../evidence/logs/003-r3-hu-info-dump.txt)
- [`003-r3-stage6-size-override.txt`](../evidence/logs/003-r3-stage6-size-override.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`003-r3-hu-info-dump.txt`:
```
649.344 [altscreen111] SessionSetup target mprotect RW denied page=782d2000 size=4096 errno=13 Permission denied; trying anonymous MAP_FIXED clone
```

`003-r3-stage6-size-override.txt`:
```
root@mmx:/tmp> echo 1010 376 > /tmp/mibr-alt111-audi.size
```

**Finding.** Direct `mprotect` of the target page is denied (errno 13); the hook falls back to an anonymous `MAP_FIXED` clone, which works. The size override file is honoured.

**Limits.** The clone fallback is a workaround whose noise appears in later logs (filtered with `grep -v mprotect`).

**Related docs.** [architecture.md](../docs/architecture.md), [ssh-environment.md](../docs/ssh-environment.md)
