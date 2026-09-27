# Status

## Specification

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
- Q hooks `ScreenStreamCreate` and reads state through its second argument at `(*arg1)+0x38`.
- Q DIO loader has `s_sessionScreenCreatedCallback -> AirPlayReceiverSessionScreenSetDelegate`.
- Q native DIO path resolves `ScreenCreate -> ScreenRegisterDelegates -> ScreenRegister`.
- Q resolves OMX initialize/configure/start/decode/stop/visibility.
- MHI2 stock frame path is `ProcessFrames -> ScreenStreamProcessData -> ScreenStream/_ScreenStreamDecodeAll -> NvssVideoImpl::decode`.
- MU0678 `SetTimeSynchronizer` copies 12 bytes to `screen+0x20` and ignores extra args.

## Current MHI2 implementation already contains

Type 111 detection, second SessionScreen, UpdateState, ScreenSetup, AES derivation, SetSecurityInfo, secondary listener, 111 response descriptor, accept worker, NetSocket wrapper, ProcessFrames, and an NvSS decode tee.

## Q divergences

- ScreenSetup can receive zero as arg 3; Q passes the real Create result.
- Current socket code abstracts the listener more simply; Q uses seven arguments.
- Q captures `(*arg1)+0x38` after ScreenStreamCreate; current MHI2 does not reproduce it.
- Q binds new SessionScreens through the DIO callback/delegate mechanism; current MHI2 uses different repoint/copy bookkeeping.
- Current MHI2 manual UUID/client/end-time setters are not part of Q cluster's resolved API surface and should not define the replica lifecycle without proof.
- Q uses OmxVideoImpl; MHI2 uses NvssVideoImpl. Translate only the decoder primitive.

## Runtime gaps

No complete successful live 111 run has shown the entire path through secondary decode/display. The raw 857727-byte stock MU0678 `libairplay.so` is not currently in the active file set, leaving some exact stock-MHI2 internal ABIs unresolved.

## Deferred safety

Q SHA/layout fingerprint and patch refusal controls are documented separately and intentionally deferred until functionality is established.
