# Native Screen B and `vc_render`

How stream 111 gets from the phone to the cluster. Source: `src/gen2-audi/` (hook) and `src/vc_render/`.

## The five things that had to be right

1. **A reachable port.** The unit's `pf` ruleset admits inbound TCP on `carplay0` only for 5000:5001, 5010, 6000, 6030,
   7000:7001, 7100 and then `block drop in on carplay0 all`. Our first 111 listener on 6031 was silently dropped (the phone
   waited, `kTimeoutErr -6722`). Stream 111 now listens on **6000**. [TRACE-006](../traces/TRACE-006-unit-packet-filter.md)
2. **The stock listener recipe.** `ServerSocketOpen` (0x79910 in this build) is a normal seven-argument AAPCS call; the working
   screen/media recipe is the stock caller at 0x23380: `arg1 = *(uint8_t*)(session+0x51)`, `arg2 = 1`, `arg3 = 6`,
   `arg4 = 0xFFFFE872`, `arg6 = 0xFFFD4000`, fresh output storage for `arg5/arg7`. The 0x23C48 TCP call is the RAOP event
   listener, not ours.
3. **A genuine second SessionScreen** built with the stock machinery (`Screen_Create`, `UpdateState`, `Setup`,
   `SetSecurityInfo`, `ProcessFrames`), with a **stub video sink**: only objects created on B's start thread while
   `g_nb_starting` is set are treated as B; everything else forwards to the real `NvssVideoImpl`. The hook refuses B if the ABI
   gates fail (fail-closed). [TRACE-007](../traces/TRACE-007-native-screen-b.md)
4. **Order.** Stock stream A must start/configure before B opens; B must survive a 110-only teardown. [TRACE-010](../traces/TRACE-010-stock-a-b-ordering.md)
5. **A separate renderer.** Opening a second NvSS instance inside `dio_manager` makes stock `NvSSVideoClose` hang on leaving
   CarPlay, so B only **forwards** frames (`NATIVE-B FORWARD MODE ON`) to `vc_render` on `127.0.0.1:19830`. [TRACE-011](../traces/TRACE-011-vc-render-forward-mode.md)

## NvssVideoImpl layout used for interposition (this libairplay build) [static]

`libairplay.so` SHA-256 `a3ab0fe0ac20a712f7f240c1abd852a38faf78f2673880342d0eb24a6d8c29be`; vtable at 0xa92e0, object size 0x54,
NvSS handle at +0x50, `ScreenStreamCreate` at 0x7f544.

| vptr offset | method | address |
| --- | --- | --- |
| +0x00 / +0x04 | D1 / D0 | 0x8019c / 0x80204 |
| +0x08 | initialize | 0x80198 |
| +0x0c | finalize | 0x80188 |
| +0x10 | configure | 0x7feb4 |
| +0x14 | start | 0x7fd2c |
| +0x18 | stop | 0x7fc2c |
| +0x1c | decode | 0x7faf0 |
| +0x20 | setVisibility | 0x7f890 |

This differs from earlier notes (the start/stop/configure order). `createVideoImpl` (0x7f784) has a PC-relative prologue and
cannot be inline-hooked, so the exported `NvssVideoImpl` methods are interposed instead.

## `vc_render` wire format and NvSS sequence

Input over loopback: `u32 'AV11' | u32 len | u64 ts | u32 flag | len bytes (Annex-B AU)`, little-endian.
NvSS sequence recovered from the stock implementation: `kdInitializeNV(); NvSSVideoOpen(&h, params[0x18]);
NvSSVideoStreamConfigure(h, {w,h,fps}); NvSSVideoGetAttribs(h, buf[0x40]); NvSSVideoDecode(h, {ptr,len,pad,ts64,flag}, 0);
NvSSVideoClose(h)`. Defaults `outputDevice 1`, layer 58, 1440×456 (the cluster raster is 1440×542; `--h 542` is used on the car).
It also prints `STATS` every 5 s and supports timestamp modes `raw|wall|zero` (`wall` is used).

## Switches (all files; the hook reads the `/tmp` and persistent `/mnt/app/root` variants)

| File | Effect |
| --- | --- |
| `/tmp/mibr-alt111-audi.native-b` (or `/mnt/app/root/mibr-carplay111-native-b`) | enable native Screen B |
| `…audi.native-b-fwd` | forward frames to `vc_render` instead of decoding in `dio_manager` |
| `…audi.stage` (`7` default since r24) | display clone + knob HID + primary input → `features=2` |
| `…audi.size` / `/mnt/app/root/mibr-carplay111-size` | canvas size (`1440 542`) |
| `…audi.adj` | `updateViewArea` adjacency mode (default 1 since r20) |
| `/mnt/app/root/mibr-carplay111-safearea*.conf` | safe areas — [safe-areas.md](safe-areas.md) |

`mount -uw /mnt/app` is required before writing the persistent ones; `/tmp` is cleared at boot.

## Known limits

- Replugged sessions report 15–17 fps in the hook's `NATIVE-B rx` counter while the picture looks smooth; cold boot reports 29–30.
- The first session after boot is black if the phone UI was acquired before the car's navigation was ready — see
  [runtime-lifecycle.md](runtime-lifecycle.md).
- Two re-acquires of the phone UI in a row leave the phone throttled at ~16 fps.
