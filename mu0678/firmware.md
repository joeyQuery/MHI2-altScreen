# Firmware identity and hashes — Audi MHI2 MU0678

Test car: 2016 Audi A4 (B9) with Virtual Cockpit. Head unit `MU0678`, train `MHI2_ER_AUG22_P3241`. **One car, one firmware.** Nothing here is a compatibility claim for other units.

## Identity (proven from the unit's own files)

- Project `project_mmx2_audi_er_g22.cfg`, OEM `AU`, region `ER`, variant `G22`, Framework 5.41.10.SR5 (branch `rel/CLU6`), HMI `HMI_AU_MIB2_H_EU_G22_C6_160601`, Nvidia 17.37.03.
- Part/label `MU0678`, `P3241`. The audited upstream Audi build is `K3346 / MU1438`; P3241 is a **different train**, so MU1438 evidence is "close", not "identical", wherever a hash below differs.
- The live `/etc/eso/production/displaymanager.json` pulled from the car is byte-identical to the dump copy (`c2263eb3…`).
- Cluster `8W5920790C`; native Virtual Cockpit map = **1440×455** (negotiated as 1440×456 because H.264 4:2:0 needs even sizes).

## Hashes compared with upstream evidence

| Item | MU0678 (this car) | Upstream Audi MU1438 | Škoda MU1440 ref |
| --- | --- | --- | --- |
| `displaymanager.json` | `c2263eb3…` | identical | differs |
| `dio_manager.json` | `99a11a14…` | identical | differs |
| `smartphone_integrator.json` | `6e7968e0…` | identical | differs |
| `libairplay.so` (whole file) | `a3ab0fe0…` (857 727 B) | `34503a9f…` (857 735 B) | `193a4fd9…` |
| 16 audited AirPlay functions | — | 16 / 16 identical | 1 / 16 |
| `dio_manager`, `smartphone_integrator` binaries | `0ca317e0…`, `80d8b7dd…` | differ (`bd42c9ee…`, `dabdac3f…`) | differs |

The config profile and the AirPlay receiver logic match the audited Audi build; whole-file `libairplay.so` differs by 8 bytes (virtual addresses shift uniformly). Evidence: [`evidence/libairplay_mu0678.json`](../evidence/libairplay_mu0678.json), regenerate with [`tools/gen_libairplay_evidence.py`](../tools/gen_libairplay_evidence.py).

## Hook-site prologues (the hook fails closed on mismatch)

| Function | VA | size | first words | gate |
| --- | --- | ---: | --- | --- |
| `AirPlayReceiverSessionSetup` | 0x22fd8 | 5032 | e92d4ff0 ed2d8b02 | PASS |
| `AirPlayReceiverSessionStart` | 0x21360 | 2992 | e92d4ff0 ed2d8b02 | PASS |
| `AirPlayReceiverSessionTearDown` | 0x22b1c | 1004 | e92d4ff0 ed2d8b02 | PASS |
| `AirPlayReceiverSessionPlatformControl` | 0x1b710 | 2392 | e92d4ff0 ed2d8b04 | PASS |
| `AirPlayReceiverSessionControl` | 0x24d88 | 1660 | e92d4ff0 e1a06002 | PASS |

Mounts, devices and process list from the unit: [`evidence/mounts.txt`](../evidence/mounts.txt), [`evidence/devs.txt`](../evidence/devs.txt), [`evidence/pidin.txt`](../evidence/pidin.txt) (VIN/IMSI redacted).

See also the [static map](static-map.md).
