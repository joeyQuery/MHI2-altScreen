# TRACE-017 — `dio_manager` missing: triage

**Question.** `dio_manager` was not running once. Was it the hook?

**What we did.** Collected `pidin`, USB insertion and `smartphone_integrator` logs.

**Evidence.**

- [`017-dio-manager-missing-triage.txt`](../evidence/logs/017-dio-manager-missing-triage.txt)
- [`017-smartphone-integrator-sloginfo.txt`](../evidence/logs/017-smartphone-integrator-sloginfo.txt)
- [`017-usb-insertion.txt`](../evidence/logs/017-usb-insertion.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`017-smartphone-integrator-sloginfo.txt`:
```
Jan 01 00:00:09    5 10000     0 00:00:03.790 servicemgr BENCH_Startup 2 930 [SM] AS_INITIALIZING reported for smartphone_integrator
```

`017-usb-insertion.txt`:
```
  966700   1 sbin/io-usb         10r SIGWAITINFO
```

**Finding.** Cause undetermined; not shown to be the hook.

**Limits.** Seen once.

**Related docs.** [open-issues.md](../docs/open-issues.md)
