# Replica plan

The functional reference is the audited MHI2Q type-111 path in `docs/mhi2q-reference-package-2026-09-27.md`.

## Architecture first: reproduce the working Q boundary

The authoritative Q package does **not** replace `libairplay.so`. Its installer verifies the exact expected AirPlay ABI and installs a custom `dio_manager.mhi2q_carplay_loader` as the DIO application, with `libmhi2qcarplaycluster.so` as a linked cluster component.

Therefore the MU0678 implementation target is:

```
stock MU0678 libairplay.so
        ^
        | existing AirPlay ABI
        |
MU0678-compatible DIO/CarPlay loader
        |
        +-- 111 detection / secondary SessionScreen
        +-- secondary listener / accept worker
        +-- decoder/output integration
        +-- dedicated VC display sink
```

Do not make replacement of `libairplay.so` the default implementation strategy.

## Functional lifecycle

1. Preserve 110 and detect 111.
2. Create B and call `UpdateState(B,1)`.
3. Call MU0678 `ScreenSetup` using its native ABI. Q passes its CreateResult as arg3, but MU0678 `0x26400` overwrites caller r2 with internal sp+4 output storage, so no third-argument CreateResult dependency is required.
4. Preserve Q time/security ordering using MU0678-native structure offsets and negotiated-value representation.
5. Construct the MHI2 secondary listener using the **stock MU0678 screen/media ServerSocketOpen recipe** recovered at `0x23380`:
   - arg1 = `*(uint8_t *)(session+0x51)`
   - arg2 = `1`
   - arg3 = `6`
   - arg4 = `0xFFFFE872`
   - arg5 = fresh bound-result output storage
   - arg6 = `0xFFFD4000`
   - arg7 = fresh socket-fd output storage
6. Preserve the Q-proven 111/dataPort response and secondary accept.
7. Wrap the accepted native socket with the MU0678 equivalent of `NetSocket_CreateWithNative`.
8. In the post-accept worker, call `StartSession(B,...)` and then `ProcessFrames(B,socket)`.
9. Reproduce the Q SessionScreen-created DIO delegate relationship once the external dispatcher is proven; do not invent a local callback trigger.
10. Map Q's native ScreenCreate/RegisterDelegates/Register surface to the exact MU0678 equivalent.
11. Translate Q OMX video operations to the MU0678 NvSS decoder primitive only; do not assume the surrounding Q object layout.
12. Feed the secondary decoded stream into a dedicated MU0678 output sink rather than changing global `ScreenCopyMain()` behaviour.
13. On teardown, preserve the Q-proven order: original AirPlay `SessionTearDown` first, then secondary listener close, accept-worker join, `ScreenStreamStop(streamB)`, `StopSession(screenB)`, `ScreenDeregister(vc_screen)`, and duplicate-state clearing.
14. Keep the stock 110 path unchanged throughout.

## Implementation prerequisite

Before writing a MU0678 DIO replacement, audit the stock MU0678 `/eso/bin/apps/dio_manager` startup/linkage and the production DIO configuration. The objective is to determine whether a Q-style replacement DIO executable can be loaded on MU0678 while preserving all ordinary DIO behaviour.

Only after that audit should the exact loader/cluster implementation be selected.

## Explicitly not required by current evidence

- Do not add `StartSession(B)` merely because Q resolves the symbol.
- Do not reproduce Q's `ScreenStreamCreate` `(*arg1)+0x38` capture unless a later xref proves it is functionally required.
- Do not use the RAOP event listener recipe at `0x23C48` for the secondary screen listener.
- Do not copy Q's `session+0x55`, `0x17D4`, or `-1` listener values into MHI2.
- Do not replace `libairplay.so` merely to imitate the Q implementation.

