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

## Current MHI2 implementation already contains

Type 111 detection, second SessionScreen, UpdateState, ScreenSetup, AES derivation, SetSecurityInfo, secondary listener, 111 response descriptor, accept worker, NetSocket wrapper, ProcessFrames, and an NvSS decode tee.

## Q divergences / required platform translation

- Q passes its Create result as ScreenSetup arg 3, but MU0678 ScreenSetup @ 0x26400 overwrites caller r2 with internal sp+4 storage, so there is no MHI2 third-argument dependency.
- Q and MHI2 both use the seven-argument AAPCS ServerSocketOpen shape; MHI2 requires the native screen/media values recovered from stock caller 0x23380.
- Q's +0x38 ScreenStream capture is diagnostic/unproven functional state and should not be copied solely for parity.
- Q loader DIO creation/registration operations are proven, but their exact linkage to the type-111 SessionScreen is across a callback boundary.
- Current MHI2 manual UUID/client/end-time setters are not part of Q cluster's resolved API surface and should not define the replica lifecycle without proof.
- Q uses OmxVideoImpl; MHI2 uses NvssVideoImpl. Translate only the decoder primitive.
- The Q helper launch path is now proven at the process-call level, but the exact r4 path data-flow at spawnl and the exact frame handoff into the helper remain to be closed.

## Remaining trace targets

- exact semantic values and stack construction for stock MHI2 ServerSocketOpen arguments 1/2/4/14/15/16;
- exact external dispatcher that invokes s_sessionScreenCreatedCallback;
- exact r4 path data-flow at Q cluster spawnl 0x4578;
- exact Q frame handoff to mhi2q-aa-cluster-display;
- exact physical display mapping on MHI2;
- successful live 111 run through secondary decode/display.

## Static closure

The socket boundary is no longer unresolved: ABI shape and the exact stock MHI2 screen/media listener recipe are proven in traces/016-static-closure-mhi2-listener.md. Q teardown ordering is also proven: original AirPlay SessionTearDown first, then secondary listener close, worker join, ScreenStreamStop, StopSession, ScreenDeregister, and duplicate-state clearing.

The Q output boundary is now substantially closed. The working cluster contains a real spawnl call with --live-carplay, the packaged helper recognizes that mode, selects 1440x540 geometry, and directly creates/posts QNX Screen windows. Therefore the helper is no longer merely a static candidate; its launch edge is proven. The remaining uncertainty is the exact argument/data-flow and physical output mapping.

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

The MU0678 StartSession limitation is proven, but the Q output boundary is now narrower and more precise: the working package contains a separate CarPlay-aware display executable, and the Q CarPlay cluster has a real spawnl launch edge using --live-carplay. A second native Screen cannot be assumed to be the complete Q replica.

MU0678 AirPlayReceiverSessionScreen_StartSession @ 0x25fd0 creates the ScreenStream through ScreenStreamCreate, then obtains the native main Screen and clones its properties into the newly created stream before ScreenStreamStart. The StartSession argument #2 is configuration/context input; it is not a native Screen selector.

Therefore a second MHI2 SessionScreen cannot be made to target a different native Screen merely by passing a different StartSession argument.

The correct static construction for B is the transport/decoder half plus an explicit output-boundary implementation:

1. Create SessionScreen B and complete its AirPlay setup/security state.
2. Do not rely on stock StartSession's main-Screen lookup as a selector for B.
3. Reproduce the post-accept Q StartSession/ScreenStream stage using the MU0678-native ABI.
4. Bind B's decoded output explicitly to the intended MHI2 display path; do not globally alter ScreenCopyMain().
5. Preserve B's stream state independently; the recovered ScreenStream type field is +0x94, which must identify the secondary stream as 111 before ProcessFrames.
6. Feed the accepted 111 socket to ProcessFrames(B,...).
7. If the native ScreenStream path cannot be bound to the intended output, the Q reference now gives a concrete alternative architecture: a dedicated H.264/display helper process with live-CarPlay mode and QNX Screen-window output.
8. Keep stock 110 behaviour unchanged.

### What remains genuinely unproven

The exact Q-side argument/data-flow edge from the launch helper's r4 into the embedded cluster-display path is not yet fully reconstructed, nor is the exact handoff of frame data into the helper. The physical mapping from the helper's QNX Screen window to the cluster is also not proven. Runtime validation is still required to prove simultaneous 110+111 output.

These are narrower unknowns than the previous “candidate helper” state: the existence of the launch call, live-CarPlay mode, helper display implementation, and 1440x540 geometry selection are now statically proven.

## 2026-09-27 static closure: Q live-CarPlay input transport

The packaged Q display helper is now statically tied to the cluster's loopback producer. In live-CarPlay mode, mhi2q-aa-cluster-display selects port 15557, creates an IPv4 TCP socket, and connects to 127.0.0.1:15557. The Q CarPlay cluster passes the same 15557 constant into its loopback/listener setup and contains H.264 viewer/IDR synchronization state. The cluster then launches the helper with --live-carplay.

This closes the display helper's input transport: Q does not rely on an inherited decoder pipe or stdin for live-CarPlay. The remaining static subtrace is the exact queued-access-unit writer call that feeds the 15557 socket. The final physical mapping of the helper's QNX Screen window remains runtime/hardware-specific.


## 2026-09-27 static closure: Q loopback writer-thread creation

The final local producer trace is now partially closed at the thread boundary. The cluster's loopback setup calls pthread_create at 0x4128; r0 is thread-id storage, r2 resolves to start routine 0x38fc, and r3 is the worker argument/context. The 0x38fc routine is a mutex/condition-driven worker with queue/state cleanup logic and resolved callback edges.

Important precision: 0x38fc does not directly call the imported write symbol. The library does import write and has concrete write callsites at 0x6b68 and 0xa36c, but this stripped artifact does not yet establish which callback from the 0x38fc worker performs the final 15557 access-unit write. Therefore the writer-thread existence is proven; the final indirect callback-to-write edge is not.

This is the correct remaining static boundary. Do not describe 0x38fc itself as a direct write(fd,buf,len) routine without further callback data-flow proof.
