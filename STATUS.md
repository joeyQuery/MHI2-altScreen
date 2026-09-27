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
- Q opens a second listener through a 7-argument `ServerSocketOpen` ABI.
- Q advertises `{type:111,dataPort:N}`.
- Q accepts the secondary socket, wraps it with `NetSocket_CreateWithNative`, and calls `ProcessFrames(B,socket)`.
- Q hooks `ScreenStreamCreate` and reads four bytes through its second argument at `(*arg1)+0x38`; the destination is not subsequently read in the Q cluster, so this is currently treated as diagnostic/bookkeeping rather than a required functional step.
- Q DIO loader has `s_sessionScreenCreatedCallback -> AirPlayReceiverSessionScreenSetDelegate`.
- Q resolves the native DIO Screen operations `ScreenCreate -> ScreenRegisterDelegates -> ScreenRegister`.
- Q resolves OMX initialize/configure/start/decode/stop/visibility.
- MHI2 stock frame path is `ProcessFrames -> ScreenStreamProcessData -> ScreenStream/_ScreenStreamDecodeAll -> NvssVideoImpl::decode`.
- MU0678 `SetTimeSynchronizer` copies 12 bytes to `screen+0x20` and ignores extra args.

## Current MHI2 implementation already contains

Type 111 detection, second SessionScreen, UpdateState, ScreenSetup, AES derivation, SetSecurityInfo, secondary listener, 111 response descriptor, accept worker, NetSocket wrapper, ProcessFrames, and an NvSS decode tee.

## Q divergences

- ScreenSetup can receive zero as arg 3; Q passes the real Create result.
- Current socket code uses a different argument layout; Q uses seven arguments. Equality with stock MHI2 remains unproven until the stock MU0678 `libairplay.so` callsite is available.
- Q captures `(*arg1)+0x38` after ScreenStreamCreate; current MHI2 does not reproduce it.
- Q loader contains executable `ScreenCreate`, `ScreenRegisterDelegates`, and `ScreenRegister` callsites; the exact call-by-call linkage of those operations to the type-111 SessionScreen remains unresolved.
- Current MHI2 manual UUID/client/end-time setters are not part of Q cluster's resolved API surface and should not define the replica lifecycle without proof.
- Q uses OmxVideoImpl; MHI2 uses NvssVideoImpl. Translate only the decoder primitive.

## Remaining trace targets

- exact Q `StartSession(B)` callsite and arguments;
- exact Q type-111 SessionScreen -> DIO Screen creation/registration chain;
- stock MHI2 `ServerSocketOpen` ABI;
- Q normal-session teardown ordering;
- successful live 111 run through secondary decode/display.

## Runtime gaps

No complete successful live 111 run has shown the entire path through secondary decode/display. The raw 857727-byte stock MU0678 `libairplay.so` is not currently in the active file set, leaving some exact stock-MHI2 internal ABIs unresolved.

## Deferred safety

Q SHA/layout fingerprint and patch refusal controls are documented separately and intentionally deferred until functionality is established.
