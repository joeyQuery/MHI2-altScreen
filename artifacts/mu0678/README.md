# Build outputs — MU0678

Built on the author's workstation with the QNX 6.5 ARMv7 toolchain (`qcc -Vgcc_ntoarmv7le`, `-O2 -g`, softfp) from the sources in `src/`
using `build/build_gen2_audi.sh` and `build/build_vc_render.sh`. The binaries are **unstripped** and carry no OEM code.

| File | Source | Notes |
| --- | --- | --- |
| `r60/libaltscreen111.so` | `src/gen2-audi/` (`libaltscreen111_gen2.c` SHA-256 `9ade9482e31d49fa1f78f6e9d37a844adcba37e52669a46e7918ca199a437596`) | installed on the unit as `/mnt/app/eso/lib/libmibr_carplay111.so` by `deployment/mu0678_hook_swap.sh` |
| `r60/vc_render` | `src/vc_render/vc_render.c` | copied to `/mnt/app/root/vc_render` |

Identification: the C source still carries the build-tag string `audi-mu0678-r23-rxstats` (logged as `build audi-mu0678-r23-rxstats`). The **r60** behaviour is
identified in the log by `r60 start layout from /tmp/mibr-alt111-lastlayout=N` and the `r55/r56/r58` lines — see [docs/builds.md](../../docs/builds.md).
Verify an installed hook by SHA-256 (the unit's `grep` cannot search binaries). SHA-256 values are in `SHA256SUMS`.
