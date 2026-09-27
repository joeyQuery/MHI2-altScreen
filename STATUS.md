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
- Q derives B AES material with DeriveAESKeySHA512ForScreen(session+0x1c8,16,negotiated64) and calls SetSecurityInfo.
- Q opens a second listener through its 7-argument ServerSocketOpen ABI.
- Q advertises {type:111,dataPort:N}.
- Q creates a dedicated secondary worker (`pthread_create` at `0x4128`, start routine `0x38fc`).
- That worker directly calls `SocketAccept` (`0x395c`), `NetSocket_CreateWithNative` (`0x3a70`), post-accept `StartSession(B,...)` (`0x3b60`), and `ProcessFrames(B, NetSocket, Q-context)` (`0x3bbc`).
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
- Q live-CarPlay framing is closed: the helper reads an exact 32-byte header requiring AAF1, byte 4 = 2, byte 7 = 32; the Q cluster constructs the matching 32-byte header at 0xa600 and writes it through the timed write path at 0xa1e8.
- Q live-CarPlay loopback endpoint is 127.0.0.1:15557.
- Q and MU0678 ProcessFrames context translation is closed: both use session+0x0c as the host context, but Q reads context+0x1c8 while stock MU0678 reads context+0xc8 at _ScreenThread 0x204d0.
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
- The Q helper launch path and frame handoff are now statically closed at the protocol boundary: the cluster produces the AAF1 header plus payload, and the helper parses the same header and consumes the exact payload over the loopback TCP connection.

## Remaining static trace targets

No architecture-level static trace target remains open for the Q-to-MU0678 lifecycle. The implementation-ready translation is recorded in `docs/mu0678-111-implementation-blueprint-2026-09-27.md`.

The remaining work is the final target-specific integration and hardware validation:
- bind the new MU0678 cluster into the exact MU0678 DIO image using the proven callback/interposition boundary;
- connect the 111 SETUP response to the MU0678 secondary listener/dataPort path;
- feed MU0678 NvSS encoded access units into the AAF1-compatible local output sink;
- bind the output sink to a MU0678-native Screen/DMDT route targeting DISPLAYABLE_KOMBI_MAP_VIEW;
- validate simultaneous 110+111 operation and stable teardown/reconnect.

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

The authoritative Q implementation is now understood at the correct boundary.

It preserves the stock `libairplay.so` file, loads `libmhi2qcarplaycluster.so` into the DIO process, resolves the live DIO/AirPlay APIs, and performs guarded runtime interposition with a 4-byte patch primitive.

The stock MU0678 DIO executable has the required exported host callbacks. The direct MU0678 translation map is documented in:

`docs/mu0678-external-cluster-implementation-2026-09-27.md`

The implementation target is therefore a MU0678-compatible external CarPlay cluster loaded into the existing DIO process, with MU0678-native AirPlay/Screen/NvSS calls.

The previous assumption that the working Q patch was equivalent to shipping a patched `libairplay.so` is corrected. The supplied MU0678 `libairplay_patched_v6.so` remains useful as a separate source of MU0678 internal patch-site evidence, but the authoritative Q mechanism is runtime interposition from the external cluster.

### Remaining runtime gaps

No complete successful live 111 run has shown the entire path through secondary decode/display.

The implementation still requires:

- MU0678 DIO runtime-interposition integration against the now-verified target executable;
- dedicated secondary output sink;
- hardware validation of 110+111 simultaneous operation;
- stable teardown/reconnect.

\n\n## 2026-09-27 implementation pass\n\nAn ARM EABI5 MU0678 external-cluster implementation has now been added under `src/mu0678/`. It preserves stock `libairplay.so`, resolves the existing SessionScreen lifecycle APIs dynamically, implements the MU0678-native ProcessFrames context translation (`session+0x0c` then `+0xc8`), reproduces the Q AAF1 32-byte live-output header, and exposes explicit entry points for SETUP, time-sync, accept, encoded-frame publication, and teardown.\n\nThe implementation is intentionally not labelled installable yet: the exact MU0678 DIO executable required for the final guarded interposition/SETUP trampoline is not present in the current authoritative upload, so inventing those patch sites would violate the evidence boundary. The source is therefore the implementation core, with the remaining integration boundary explicitly exposed rather than guessed.\n\n\n## Exact target executable now verified\n\nThe MU0678 target `dio_manager` is present in the repository dump at `dump/MU0678-MHI2_ER_AUG22_P3241-JTB-07429.06.161184010D/advanced/MU0678-appimg/eso/bin/apps/dio_manager`. Its Git blob SHA is `1f9dd87e2553f3a89fe083466d1ed3729dd9d094`, size 649413 bytes, and the exact callback anchors are recorded in `docs/mu0678-target-dio-audit-2026-09-27.md`. The previous statement that the target executable was unavailable is superseded.\n