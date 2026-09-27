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

## 2026-09-27 correction: Q 15557 listener vs worker

The deeper binary trace separates two previously conflated roles. Function 0xad4c is the concrete 15557 TCP listener constructor: its caller supplies 0x3cc5 (15557), and the function creates/binds/listens on the IPv4 endpoint before initializing transport state and a pthread path. The pthread at 0x4128 starts 0x38fc, but 0x38fc is not proven to be the direct H.264 socket writer; it is a broader mutex/condition-driven worker/orchestration routine with indirect callback edges.

The remaining static problem is therefore specifically the callback/data-flow from queued access units to the final socket-write primitive. The earlier wording that treated 0x38fc as the writer itself is corrected here.

## 2026-09-27 final static closure: Q loopback writer

The remaining writer boundary is now closed from the authoritative Q ELF. The imported write PLT is 0xf44. The 0x6b30/0x6b68 path is a separate file writer because 0x6b30 opens a file before writing. The loopback output path is instead 0xa738 -> 0xa844 -> 0xa1e8 -> 0xa36c -> write@0xf44.

At 0xa844 the loopback drain supplies the transport descriptor, prepared buffer, and byte count to 0xa1e8. The latter performs the partial-write loop at 0xa36c and retries the remaining bytes after transient QNX socket conditions. The surrounding 0xa738 drain maintains mode-dependent 24/32-byte framing/byte accounting and is reached from the loopback transport state path.

Therefore the previously unresolved callback-to-write edge is no longer unresolved. The Q static display transport is closed as:

111 SessionScreen -> secondary listener -> accepted stream -> Q H.264/queue state -> loopback drain 0xa738 -> 0xa844 -> 0xa1e8 -> write@0xf44 -> TCP 127.0.0.1:15557 -> mhi2q-aa-cluster-display --live-carplay -> QNX Screen window.

Remaining runtime-only questions are the physical display mapping and successful simultaneous 110+111 execution on MU0678. Do not claim the runtime descriptor identity or physical VC mapping without hardware evidence.

## 2026-09-27 final static display-routing closure

The last three Q/MHI2 display questions are now statically closed in docs/traces/018-static-closure-display-routing.md.

1. Accepted descriptor identity is closed: Q transport+0x4 is the 15557 listening descriptor; accept() at 0xa100 stores the accepted display-helper connection at transport+0; the writer at 0xa738/0xa844 passes that same transport+0 descriptor through 0xa1e8 to write() PLT 0xf44.

2. Q display routing is explicitly selected: mhi2q-aa-cluster-display sets IVRDISPLAY=SCREEN, loads libdisplayinit/libscreen/libEGL, and resolves the display/window stack. Its VC-routing code embeds and invokes the DMDT argument vector /eso/bin/apps/dmdt, dmdt, sc, 74, sb, dc, 70, 98, 101, 102, 3, 20 plus a 16-byte value. The helper's live-CarPlay geometry is 1440x540 and is Q-specific.

3. MU0678 translation is now explicit: use a parallel 111 H.264 producer with a dedicated QNX Screen/DMDT output sink, targeting the recovered MU0678 DISPLAYABLE_KOMBI_MAP_VIEW geometry 1440x455. Do not alter ScreenCopyMain() or stock 110. Do not copy Q's 1440x540 geometry or Q OMX object layout.

The exact physical meaning of a DMDT context is a hardware/runtime fact, but the logical VC/KOMBI route and the MHI2 implementation boundary are now identified.

## 2026-09-27 exact original-patch reproduction

The supplied original MU0678 patch is now authorized as the functional reference.

The pristine baseline SHA-256 is:
`a3ab0fe0ac20a712f7f240c1abd852a38faf78f2673880342d0eb24a6d8c29be`

The supplied original patched SHA-256 is:
`62eee8aacaad53be049f0e458abaf492c777da7879978a8c2320b661156954d0`

A local reproduction from the pristine baseline was generated and verified byte-for-byte identical to the supplied patched ELF.

The reproduction is deterministic and committed as:
`tools/reproduce-original-mu0678-patch.py`

The resulting patched ELF is **not yet claimed to be runtime-validated**. The remaining gate is hardware execution and observation of the complete 110+111 lifecycle.
