# Third-party notices, prior art and attribution

This project is independent research. The GPL-3.0-or-later licence applies to **original project material**; it does
not relicense third-party software, firmware, Apple components or OEM binaries.

## Attribution policy

1. **Open-source upstream work** is credited with project, repository, commit/tag where relevant, licence and what was
   used or learned. If code is incorporated, its licence terms are followed.
2. **Public technical prior art** (posts, write-ups, logs) is cited as prior art; citation does not imply endorsement.
3. **Proprietary / commercial implementations** examined for interoperability are treated as **evidence**, not as project
   source. They are not distributed here; findings are re-expressed as independent observations.

## Known upstream / prior art

### harman-f — MHI2 AltScreen CarPlay

`harman-f/mhi2_altscreen_carplay`, GPL-3.0-or-later. The GEN2 hook in `src/gen2-audi/` was ported from this project's
public GEN2 source for the Audi MU0678 (new native Screen B, vc_render, view-area, zoom and layout logic are original
to this repository). The earlier static work in this repository used its audited evidence as a baseline.

### OneB1t — VcMOSTRenderMqb

`OneB1t/VcMOSTRenderMqb` (Unlicense). Prior art for the MQB QNX → Virtual Cockpit rendering path. Not used on this car's
LVDS path, but it shaped the early hypotheses.

### chefranov — mhi2-au37x-carplay

`chefranov/mhi2-au37x-carplay`, studied commit `8968119c136b1f148486149de0950058829a2f33`. Route-guidance (RGI) hook
for the HARMAN iAP2 stack: a native `librgd_hook.so` preloaded into `mm-ipod` and an HMI jar `rgd_hook.jar`.
**No licence file was found in that repository, so none of its binaries or sources are copied here.** This repository only
ships a script that patches a copy you download yourself ([tools/patch_rgd_hook_jar.py](tools/patch_rgd_hook_jar.py),
[hud/UPSTREAM.md](hud/UPSTREAM.md)).

### Luka — CarPlay RGI / qnx65-armv7-toolchain

`luka-dev/mib2q-carplay-rgi` and `luka-dev/qnx65-armv7-toolchain`: public prior art for CarPlay route guidance on MIB2/MHI2
and the QNX 6.5 ARMv7 toolchain used for builds. Treated as prior art/external build dependency; not vendored.

### MIB Toolbox and the M.I.B. community

`jilleb/mib2-toolbox` and the wider community provided the access, tooling and platform knowledge that makes independent
MHI2 research practical. Access methods are intentionally not reproduced here.

### A third-party MHI2Q AltScreen package (static phase)

The first, static phase of this project compared the Audi unit against a working third-party MHI2Q AltScreen package ("Q").
That package is **not distributed here**. The notes in `docs/static-phase/` and `traces/static-phase/` record offsets,
call orders and hash facts as observations only, and are kept as the research record of how the design was derived.

### A friend's MU1329 package (Cinemo stack)

A package for a different head unit (MU1329, Cinemo-based `dio_manager`) was inspected to read its view-area table and the
`changeMapZoomLevel` command name. Only those facts are cited ([docs/cluster-controls.md](docs/cluster-controls.md)); no file
from it is included.

## Apple / CarPlay

Observed CarPlay/AirPlay behaviour (log lines from the phone and the hook, feature keys, command names) is documented for
interoperability. Apple system binaries are not project-owned and are not included.

## Audi / Volkswagen Group / Harman firmware

OEM firmware, the `lsd.jxe` HMI image, `displaymanager`, `libairplay.so` and similar binaries are **not mirrored**. They are
identified by path and SHA-256 in [mu0678/firmware.md](mu0678/firmware.md). Small vendor configuration excerpts
(`displaymanager.json`) are included under `evidence/config/` as research evidence.
