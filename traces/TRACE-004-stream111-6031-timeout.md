# TRACE-004 — Stream 111 accepted, then silence on 6031

**Question.** The phone accepts the stream-111 SETUP. Why does it never connect to our data port?

**What we did.** Logged the SETUP, checked our listener with `netstat`, and watched for traffic.

**Evidence.**

- [`004-r3-stream111-accepted-6031.txt`](../evidence/logs/004-r3-stream111-accepted-6031.txt)
- [`004-r3-6031-listening-no-traffic.txt`](../evidence/logs/004-r3-6031-listening-no-traffic.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`004-r3-stream111-accepted-6031.txt`:
```
1741.612 [altscreen111] GEN2 candidate active: 111=1440x456@30 physical=200x63 altPort=6031 tee=19820 capture=19821 URL=maps:/car/instrumentcluster uuid=b7e6c5a0-2222-4000-8000-000000000002 viewAreas=1 autoShow=0 bit26Mode=0
```

`004-r3-6031-listening-no-traffic.txt`:
```
root@mmx:/mnt/app/root> grep 6031 /tmp/net.txt
```

**Finding.** We answered `dataPort=6031` and listened there, but nothing ever arrived. TRACE-006 shows why.

**Limits.** Alone, this trace does not tell you the cause; it is the symptom.

**Related docs.** [display-path.md](../docs/display-path.md), [disproven.md](../docs/disproven.md)
