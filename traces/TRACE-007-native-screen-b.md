# TRACE-007 — Native Screen B reaches H.264 decode

**Question.** Can the stock `dio_manager` receive stream 111 if we give it a stub video sink?

**What we did.** Registered a stub `NvssVideoImpl` and a native Screen B, then counted NAL units arriving.

**Evidence.**

- [`007-r5-native-b-ports.txt`](../evidence/logs/007-r5-native-b-ports.txt)
- [`009-nvss-vtable-notes.txt`](../evidence/logs/009-nvss-vtable-notes.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`007-r5-native-b-ports.txt`:
```
190.629 [altscreen111] GEN2 stream111 SETUP accepted cid=13688592430774634056 dataPort=6000 response=cloned-request+streamID111; UI acquisition armed
```

`009-nvss-vtable-notes.txt`:
```
NvssVideoImpl vtable
```

**Finding.** With the native-b marker files set, stream 111 is accepted on port 6000 (the allowed port) and handed to the native Screen B path. The vtable notes record the `NvssVideoImpl` layout the stub sink implements.

**Limits.** Decode counts (SPS/PPS/IDR) are not in this log excerpt; they were observed in the same session but not preserved here.

**Related docs.** [native-screen-b.md](../docs/native-screen-b.md)
