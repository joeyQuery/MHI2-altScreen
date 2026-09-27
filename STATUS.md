# Status

The working MHI2Q AltScreen package is the functional specification. The objective is a literal functional replica on MHI2 with only unavoidable platform-specific translations.

## Proven

- 110 is the normal/main CarPlay stream.
- 111 is the alternate/secondary CarPlay stream.
- Q dynamically resolves the AirPlay SessionScreen lifecycle.
- Q SETUP handling identifies stream type 111.
- Q creates a second SessionScreen.
- Q calls UpdateState(B,1).
- Q calls Setup(B,stream111,CreateResult); argument 3 is the actual Create result.
- Q calls SetTimeSynchronizer before security completion.
- Q derives B AES material with DeriveAESKeySHA512ForScreen(session+0x1c0,16,negotiated64) and calls SetSecurityInfo.
- Q opens a second listener through its 7-argument ServerSocketOpen ABI.
- Q advertises {type:111,dataPort:N}.
- Q accepts the secondary socket, wraps it with NetSocket_CreateWithNative, and calls ProcessFrames(B,socket).
- Q resolves StartSession, but the proven type-111 setup block does not call it.
- Q ScreenStreamCreate observes (*arg1)+0x38; the captured value is not subsequently read in the Q cluster, so this is not proven functionally required.
- Q DIO callback s_sessionScreenCreatedCallback -> AirPlayReceiverSessionScreenSetDelegate is proven.
- Q DIO loader contains executable ScreenCreate, ScreenRegisterDelegates, and ScreenRegister callsites.
- The exact callback trigger is a cross-module boundary; the loader itself does not contain a direct type-111 call into that callback.
- Q resolves OMX initialize/configure/start/decode/stop/visibility.
- MHI2 stock frame path is ProcessFrames -> ScreenStreamProcessData -> ScreenStream/_ScreenStreamDecodeAll -> NvssVideoImpl::decode.
- MU0678 SetTimeSynchronizer copies 12 bytes to screen+0x20 and ignores extra args.
- Stock MU0678 ServerSocketOpen at 0x79910 is a normal seven-argument AAPCS call. The relevant screen/media listener is the stock 0x23380 call: arg1=*(uint8_t *)(session+0x51), arg2=1, arg3=6, arg4=0xFFFFE872, arg6=0xFFFD4000, with fresh arg5/arg7 output storage. The 0x23C48 TCP call is the RAOP event listener (-5001).
- Q libmhi2qcarplaycluster.so imports spawnl and has a direct callsite at 0x4578 that passes P_WAIT-shaped mode 1 and the embedded --live-carplay argument.
- Q libmhi2qcarplaycluster.so embeds /eso/bin/apps/mhi2q-aa-cluster-display.
- Q mhi2q-aa-cluster-display directly compares argv[1] with --live-carplay at 0x103140 and selects a 1440x540 geometry state for that mode.
- The Q display helper dynamically resolves QNX Screen/display APIs including screen_create_window, screen_create_window_type, screen_create_window_buffers, screen_post_window, and destruction/property APIs.
- The helper also contains usage: %s stream.h264, establishing an H.264 input mode in addition to its live-CarPlay mode.

## Current MHI2 reverse-engineering work has established

The MHI2 analysis has statically established the Type 111 detection, second SessionScreen lifecycle, UpdateState, ScreenSetup ABI, AES derivation, SetSecurityInfo, secondary listener, 111 response descriptor, accept worker, NetSocket wrapper, ProcessFrames, and an NvSS decode tee. This repository is the audited research/specification repository, and it now also records an exact reproduction of the supplied original MU0678 patch.

## Q divergences / required platform translation

- Q passes its Create result as ScreenSetup arg 3, but MU0678 ScreenSetup @ 0x26400 overwrites caller r2 with internal sp+4 storage, so there is no MHI2 third-argument dependency.
- Q and MHI2 both use the seven-argument AAPCS ServerSocketOpen shape; MHI2 requires the native screen/media values recovered from stock caller 0x23380.
- Q's +0x38 ScreenStream capture is diagnostic/unproven functional state and should not be copied solely for parity.
- Q loader DIO creation/registration operations are proven, but their exact linkage to the type-111 SessionScreen is across a callback boundary.
- Current MHI2 manual UUID/client/end-time setters are not part of Q cluster's resolved API surface and should not define the replica lifecycle without proof.
- Q uses OmxVideoImpl; MHI2 uses NvssVideoImpl. Translate only the decoder primitive.
- The Q helper launch path is now proven at the process-call level, but the exact r4 path data-flow at spawnl and the exact frame handoff into the helper remain to be closed.

## Remaining static trace targets

No architecture-level static trace target remains open for the Q-to-MU0678 lifecycle. The implementation-ready translation is recorded in `docs/mu0678-111-implementation-blueprint-2026-09-27.md`.

The remaining work is executable implementation and hardware validation:
- reproduce/install the exact supplied MU0678 patch;
- validate the external DIO/display binding on hardware;
- validate the dedicated 111 output sink;
- demonstrate simultaneous 110+111 operation and stable teardown/reconnect.

## Static closure

The socket boundary is no longer unresolved: ABI shape and the exact stock MHI2 screen/media listener recipe are proven in traces/016-static-closure-mhi2-listener.md. Q teardown ordering is also proven: original AirPlay SessionTearDown first, then secondary listener close, worker join, ScreenStreamStop, StopSession, ScreenDeregister, and duplicate-state clearing.

The Q output boundary is statically closed. The authoritative package proves the helper launch, live-CarPlay loopback input, accepted-descriptor identity, QNX Screen/display stack, and DMDT VC-routing invocation. The remaining uncertainty is hardware-specific physical mapping and MU0678 runtime behaviour.

## Runtime gaps

No complete successful live 111 run has shown the entire path through secondary decode/display.

## Deferred safety

Q SHA/layout fingerprint and patch refusal controls are documented separately and intentionally deferred until functionality is established.

## 2026-09-27 correction / MHI2Q reference audit

The canonical repository is joeyQuery/MHI2-AltScreen. The MHI2Q reference has been audited against the supplied implementation package and recorded in docs/mhi2q-reference-package-2026-09-27.md.

The important implementation rule is now explicit: Q preserves the 110 path and constructs/drives a parallel 111 SessionScreen path. The proven Q type-111 setup sequence does not call AirPlayReceiverSessionScreen_StartSession, despite resolving that symbol. The Q ScreenStreamCreate +0x38 observation is diagnostic and not yet a proven functional dependency.

Do not copy Q offsets or socket arguments directly into MHI2. The MHI2 replica must reproduce the Q operations at MU0678-native ABI/object boundaries.

## Final static SessionScreen association

AirPlayReceiverSessionScreen_StartSession @ 0x25fd0 creates the stream and then obtains the native main Screen to clone display geometry/properties into that stream. No hidden SessionScreen->native-Screen selector remains identified. The secondary 111 path therefore requires an explicit native Screen selection/property-cloning step for B; global ScreenCopyMain() changes are not acceptable because they would affect 110.

## Final static conclusion — 2026-09-27

The authoritative MHI2Q implementation is **not a libairplay replacement**.

The supplied `install.zip` verifies and preserves the target `/eso/lib/libairplay.so` ABI. Its CarPlay implementation is delivered through a replacement DIO executable (`dio_manager.mhi2q_carplay_loader`) and `libmhi2qcarplaycluster.so`, which dynamically resolve the existing AirPlay SessionScreen/Screen APIs.

Therefore the correct MU0678 implementation target is:

```
stock MU0678 libairplay.so
        |
        +-- existing 110/main AirPlay path
        |
        +-- DIO-side extension / loader
                |
                +-- detect 111
                +-- SessionScreen B
                +-- Q-equivalent Setup/time/security
                +-- secondary listener + dataPort
                +-- accept + NetSocket
                +-- post-accept StartSession
                +-- ProcessFrames
                +-- independent decode/output sink
```

The supplied `libairplay_patched_v6.so` remains useful only as evidence of one possible MU0678 binary-injection mechanism. It is not the functional Q reference and must not be described as such.

### Corrected implementation gate

The next static implementation task is to audit the stock MU0678 `/eso/bin/apps/dio_manager` startup/linkage and production DIO configuration. We need to establish exactly how a Q-style external DIO loader can coexist with the existing MU0678 DIO responsibilities.

Only after that should the MU0678 loader/cluster binary be constructed.

The previously established MU0678 AirPlay/SessionScreen/socket/NvSS traces remain valid as the native ABI map. They are the implementation substrate the external loader must call.

### Remaining runtime gaps

No complete successful live 111 run has shown the entire path through secondary decode/display.

The implementation still requires:

- MU0678 DIO loader integration;
- dedicated secondary output sink;
- hardware validation of 110+111 simultaneous operation;
- stable teardown/reconnect.

