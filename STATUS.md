# Status

The working MHI2Q AltScreen package is the functional specification. The objective is a literal functional replica on MHI2 with only unavoidable platform-specific translations.

## Proven

- `110` is the normal/main CarPlay stream.
- `111` is the alternate/secondary CarPlay stream.
- Q dynamically resolves the AirPlay SessionScreen lifecycle.
- Q SETUP handling identifies stream type `111`.
- Q creates a second SessionScreen.
- Q calls `UpdateState(B,1)`.
- Q calls `Setup(B,stream111,CreateResult)`; argument 3 is the actual Create result.
- Q calls `SetTimeSynchronizer` before security completion.
- Q derives B AES material with `DeriveAESKeySHA512ForScreen(session+0x1c0,16,negotiated64)` and calls `SetSecurityInfo`.
- Q opens a second listener through its 7-argument `ServerSocketOpen` ABI.
- Q advertises `{type:111,dataPort:N}`.
- Q accepts the secondary socket, wraps it with `NetSocket_CreateWithNative`, and calls `ProcessFrames(B,socket)`.
- Q resolves `StartSession`, but the proven type-111 setup block does not call it.
- Q `ScreenStreamCreate` observes `(*arg1)+0x38`; the captured value is not subsequently read in the Q cluster, so this is not proven functionally required.
- Q DIO callback `s_sessionScreenCreatedCallback -> AirPlayReceiverSessionScreenSetDelegate` is proven.
- Q DIO loader contains executable `ScreenCreate`, `ScreenRegisterDelegates`, and `ScreenRegister` callsites.
- The exact callback trigger is a cross-module boundary; the loader itself does not contain a direct type-111 call into that callback.
- Q resolves OMX initialize/configure/start/decode/stop/visibility.
- MHI2 stock frame path is `ProcessFrames -> ScreenStreamProcessData -> ScreenStream/_ScreenStreamDecodeAll -> NvssVideoImpl::decode`.
- MU0678 `SetTimeSynchronizer` copies 12 bytes to `screen+0x20` and ignores extra args.
- Stock MU0678 `ServerSocketOpen` at `0x79910` is a normal seven-argument AAPCS call. The relevant screen/media listener is the stock `0x23380` call: `arg1=*(uint8_t *)(session+0x51)`, `arg2=1`, `arg3=6`, `arg4=0xFFFFE872`, `arg6=0xFFFD4000`, with fresh arg5/arg7 output storage. The `0x23C48` TCP call is the RAOP event listener (`-5001`).

## Current MHI2 implementation already contains

Type 111 detection, second SessionScreen, UpdateState, ScreenSetup, AES derivation, SetSecurityInfo, secondary listener, 111 response descriptor, accept worker, NetSocket wrapper, ProcessFrames, and an NvSS decode tee.

## Q divergences / required platform translation

- Q passes its Create result as ScreenSetup arg 3, but MU0678 `ScreenSetup @ 0x26400` overwrites caller `r2` with internal `sp+4` storage, so there is no MHI2 third-argument dependency.
- Q and MHI2 both use the seven-argument AAPCS ServerSocketOpen shape; MHI2 requires the native screen/media values recovered from stock caller `0x23380`.
- Q's `+0x38` ScreenStream capture is diagnostic/unproven functional state and should not be copied solely for parity.
- Q loader DIO creation/registration operations are proven, but their exact linkage to the type-111 SessionScreen is across a callback boundary.
- Current MHI2 manual UUID/client/end-time setters are not part of Q cluster's resolved API surface and should not define the replica lifecycle without proof.
- Q uses OmxVideoImpl; MHI2 uses NvssVideoImpl. Translate only the decoder primitive.

## Remaining trace targets

- exact semantic values and stack construction for stock MHI2 ServerSocketOpen arguments 1/2/4/14/15/16;
- exact external dispatcher that invokes `s_sessionScreenCreatedCallback`;
- Q normal-session teardown ordering;
- successful live 111 run through secondary decode/display.

## Static closure

The socket boundary is no longer unresolved: ABI shape and the exact stock MHI2 screen/media listener recipe are proven in `traces/016-static-closure-mhi2-listener.md`. Q teardown ordering is also proven: original AirPlay `SessionTearDown` first, then secondary listener close, worker join, `ScreenStreamStop`, `StopSession`, `ScreenDeregister`, and duplicate-state clearing.

## Runtime gaps

No complete successful live 111 run has shown the entire path through secondary decode/display.

## Deferred safety

Q SHA/layout fingerprint and patch refusal controls are documented separately and intentionally deferred until functionality is established.


## 2026-09-27 correction / MHI2Q reference audit

The canonical repository is `joeyQuery/MHI2-AltScreen`. The MHI2Q reference has been audited against the supplied implementation package and recorded in `docs/mhi2q-reference-package-2026-09-27.md`.

The important implementation rule is now explicit: Q preserves the 110 path and constructs/drives a parallel 111 SessionScreen path. The proven Q type-111 setup sequence does **not** call `AirPlayReceiverSessionScreen_StartSession`, despite resolving that symbol. The Q `ScreenStreamCreate` `+0x38` observation is diagnostic and not yet a proven functional dependency.

Do not copy Q offsets or socket arguments directly into MHI2. The MHI2 replica must reproduce the Q operations at MU0678-native ABI/object boundaries.
