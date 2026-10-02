# TRACE-006 — The unit packet filter

**Question.** Why did the phone never reach port 6031?

**What we did.** Dumped the live `pf` ruleset and the listening sockets.

**Evidence.**

- [`006-unit-pf-ruleset.txt`](../evidence/logs/006-unit-pf-ruleset.txt)
- [`006-unit-netstat.txt`](../evidence/logs/006-unit-netstat.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`006-unit-pf-ruleset.txt`:
```
pass in quick on carplay0 proto tcp from any to any port 5000:5001 allow-opts
pass in quick on carplay0 proto tcp from any to any port = 5010 allow-opts
pass in quick on carplay0 proto tcp from any to any port = xserver allow-opts
pass in quick on carplay0 proto tcp from any to any port = 6030 allow-opts
pass in quick on carplay0 proto tcp from any to any port 7000:7001 allow-opts
pass in quick on carplay0 proto tcp from any to any port = 7100 allow-opts
pass in quick on carplay0 proto udp from any to any port 67:68 allow-opts
pass in quick on carplay0 proto udp from any to any port 6001:6002 allow-opts
```

`006-unit-netstat.txt`:
```
root@mmx:/mnt/app/root> netstat -an | grep LISTEN
```

**Finding.** Inbound TCP on `carplay0` is passed only for 5000:5001, 5010, 6000, 6030, 7000:7001 and 7100; everything else is dropped by a default block. Moving the data port to 6000 fixed it.

**Limits.** Rules were read from one firmware build; other builds may differ.

**Related docs.** [display-path.md](../docs/display-path.md), [carplay-altscreen.md](../docs/carplay-altscreen.md)
