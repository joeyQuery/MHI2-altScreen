# TRACE-002 — r1 flight recorder: toggles and feature policy

**Question.** Which `/info` choices change the phone's behaviour? Can we flip them without rebuilding?

**What we did.** Added marker files under `/tmp/mibr-alt111-audi.*` read by the hook (alt display on/off, stock vs patched feature policy) and recorded each run.

**Evidence.**

- [`002-r1-altdisplay-advertised.txt`](../evidence/logs/002-r1-altdisplay-advertised.txt)
- [`002-r1-features-policy-stock.txt`](../evidence/logs/002-r1-features-policy-stock.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`002-r1-altdisplay-advertised.txt`:
```
root@mmx:/mnt/app/root> rm /tmp/mibr-alt111-audi.no-alt-display
```

`002-r1-features-policy-stock.txt`:
```
root@mmx:/mnt/app/root> touch /tmp/mibr-alt111-audi.features-stock
```

**Finding.** The toggles work as marker files; they became the experiment switchboard for r1–r10.

**Limits.** The transcripts record the toggles being set, not each resulting phone-side effect.

**Related docs.** [builds.md](../docs/builds.md), [carplay-altscreen.md](../docs/carplay-altscreen.md)
