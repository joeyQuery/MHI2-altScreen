# Evidence register

Authoritative Q lifecycle reference: `docs/mhi2q-reference-package-2026-09-27.md`.

## Proven

1. `111` is the alternate stream.
2. Q creates a second SessionScreen.
3. Q calls UpdateState and ScreenSetup on B.
4. Q passes CreateResult as ScreenSetup arg 3.
5. Q derives B security with the stock AES/SHA512 helper.
6. Q opens a second listener and advertises its port as 111 dataPort.
7. Q accepts the second socket and calls ProcessFrames(B,NetSocket).
8. Q ScreenStreamCreate hook reads `(*arg1)+0x38`; the captured value is not read elsewhere in the Q cluster, so this is not proven to be a functional dependency.
9. Q DIO callback uses `AirPlayReceiverSessionScreenSetDelegate`.
10. Q DIO loader contains executable ScreenCreate/ScreenRegisterDelegates/ScreenRegister callsites.
11. Q resolves StartSession but the proven type-111 setup path does not call it.
12. Q hooks OMX lifecycle.
13. MHI2 stock processing ends in NvssVideoImpl::decode.
14. Stock MU0678 ServerSocketOpen at `0x79910` is a normal seven-argument AAPCS call; the stock `0x23380` branch is the screen/media listener.
15. MU0678 `AirPlayReceiverSessionScreen_Setup @ 0x26400` overwrites caller `r2` with `sp+4` before its only internal setup-helper call, so the caller's third argument is not consumed by this export.
16. The supplied Q DIO loader has exactly one direct `ScreenCreate` BL callsite (`0x158bd4`), followed in the same creation path by `ScreenRegisterDelegates` (`0x158e20`) and `ScreenRegister` (`0x158e28`).
17. Q packages `mhi2q-aa-cluster-display`, whose binary contains `--live-carplay` and dynamically resolves QNX screen-window APIs including `screen_create_window`, `screen_set_window_property_iv`, `screen_create_window_buffers`, and `screen_post_window`.
18. `libmhi2qcarplaycluster.so` embeds `/eso/bin/apps/mhi2q-aa-cluster-display` and `--live-carplay` alongside the 111 metadata.

## Interpretation rules

- Q's resolved StartSession symbol is **not** evidence that B must call StartSession.
- Q's `(*arg1)+0x38` capture is **not** evidence that MHI2 must reproduce that state access.
- Q DIO symbol resolution is not by itself proof of the external callback dispatcher.
- Q socket/register/stack layouts must not be copied into MHI2 where the MU0678 ABI differs.

## Unresolved

- exact cross-module/process data-flow that launches and feeds `mhi2q-aa-cluster-display` for 111;
- whether the Q native Screen object, the helper process, or both are involved in the final 111 output;
- successful complete live 111 run through secondary decode/display.
