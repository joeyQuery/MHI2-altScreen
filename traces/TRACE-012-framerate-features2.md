# TRACE-012 — Frame-rate ceiling and `features=2`

**Question.** Why was the stream capped near 16 fps?

**What we did.** Dumped the display record from the phone side and compared `features=0` vs `features=2` with a knob HID.

**Evidence.**

- [`012-features2-baseline-info.txt`](../evidence/logs/012-features2-baseline-info.txt)

**Verbatim excerpts** (redacted copies; see the header of each log file).

`012-features2-baseline-info.txt`:
```
438.202 [altscreen111] GEN2 FLIGHT DUMP command=info path=root.displays[0].features type=int value=2
```

**Finding.** The phone chose the cap. `features=2` plus a knob HID gives about 30 fps.

**Limits.** Waze still aliases; Apple Maps and Google Maps are fine.

**Related docs.** [carplay-altscreen.md](../docs/carplay-altscreen.md)
