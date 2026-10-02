# MHI2Q AltScreen reference package — audited 2026-09-27

This document records the MHI2Q implementation evidence used as the functional reference for the MHI2 replica.

## Authoritative package boundary

The supplied `install.zip` is the authoritative working implementation.

Its installer explicitly verifies the existing `/eso/lib/libairplay.so` SHA-256 and does **not** replace it:

```sh
# These dependencies are NOT replaced: hooks still require their exact ABI.
run_mmx "$TOOL" verify "$AIRPLAY_SHA" /eso/lib/libairplay.so || fail "libairplay incompatible"
```

Therefore the working implementation must be understood as:

```
/eso/bin/apps/dio_manager
        |
        +-- mhi2q_carplay loader
        |      |
        |      +-- existing libairplay.so APIs
        |      +-- libmhi2qcarplaycluster.so
        |      +-- DIO callbacks / Screen APIs
        |
        +-- 110 normal path remains in stock AirPlay
        |
        +-- 111 parallel SessionScreen / transport / output path
```

The package contains `dio_manager.mhi2q_carplay_loader`, whose NEEDED list includes `libmhi2qcarplaycluster.so` and `libairplay.so). The loader imports AirPlay Session/Screen APIs including `AirPlayReceiverSessionChangeModes`, `AirPlayReceiverSessionScreenSetDelegate`, `AirPlayReceiverSessionTearDown`, `ScreenRegisterDelegates`, `ScreenCreate`, and `ScreenRegister`.

This is the critical architectural distinction: **the working Q patch extends DIO/CarPlay externally around stock AirPlay; it does not implement the feature by replacing `libairplay.so`.**

## Core architecture

The working Q implementation is a parallel secondary-screen path. It does not replace the normal 110/main path.

```
AirPlay session
  |
  +-- 110 / primary ----------------> existing CarPlay path
  |
  +-- 111 / secondary
          |
          +-- second SessionScreen
          +-- secondary listener
          +-- secondary listener / worker
          +-- post-accept StartSession(B)
          +-- NetSocket wrapper
          +-- post-accept StartSession(B)
          +-- ProcessFrames(B, NetSocket, Q-context)
          +-- secondary ScreenStream / decoder integration
          +-- dedicated display/output path
```

## Proven Q type-111 sequence

The audited Q trace establishes this order:

1. inspect the SETUP stream collection;
2. identify `type == 111`;
3. create SessionScreen B;
4. call `UpdateState(B,1)`;
5. call `Setup(B,stream111,CreateResult)`;
6. set the time synchronizer;
7. obtain the negotiated 64-bit stream value;
8. call `AirPlay_DeriveAESKeySHA512ForScreen(session+0x1c8,16,negotiated64)`;
9. call `SetSecurityInfo(B,keyA,keyB)`;
10. open the secondary listener;
11. advertise `{type:111,dataPort:N}`;
12. accept the secondary connection;
13. wrap it with `NetSocket_CreateWithNative`;
14. call `StartSession(B,...)` in the post-accept worker;
15. call `ProcessFrames(B, NetSocket, Q-context)`;
16. integrate the resulting secondary stream with DIO/decoder/output;
17. tear the secondary path down alongside the main path.

## Important correction: StartSession is post-accept

Q dynamically resolves `AirPlayReceiverSessionScreen_StartSession`, but the **initial type-111 SETUP handler does not call it**. The separate secondary-socket worker calls it after `SocketAccept` and `NetSocket_CreateWithNative`.

The worker loads the StartSession argument 2 through:

```
worker context
  -> [r9 + 0x0c]
  -> [that object + 0x1d0]
  -> StartSession(B, arg2)
```

The binary proves this object/offset chain. The exact C++ class identity of the stripped Q cluster object is not recovered. Independently, MU0678 StartSession analysis proves that its corresponding argument #2 is configuration/context input, not a hidden native Screen-B selector.

Therefore:

- do not add `StartSession(B)` to the initial SETUP handler;
- reproduce the post-accept StartSession stage;
- resolve the MU0678 equivalent of the Q worker's argument-2 object before binding B to a native Screen.

## ScreenStreamCreate / +0x38

The Q hook at approximately `0x4710` observes `(*arg1)+0x38`.

The captured value is not subsequently read by the Q cluster, so this field is currently classified as diagnostic/unproven functional state. It must not be copied into MHI2 solely for parity.

## DIO/native Screen binding

Q resolves executable callsites for:

- `ScreenCreate`
- `ScreenRegisterDelegates`
- `ScreenRegister`

Q also contains `s_sessionScreenCreatedCallback`, which calls `AirPlayReceiverSessionScreenSetDelegate`.

The exact external dispatcher that causes the callback to run remains an inter-module boundary. Symbol presence alone is not treated as proof of the complete type-111 native-Screen construction chain.

## Security

The Q implementation uses the stock AirPlay key derivation primitive for the secondary SessionScreen:

```
session security material
        +
length 16
        +
111 negotiated 64-bit value
        |
        v
AirPlay_DeriveAESKeySHA512ForScreen
        |
        +--> key A
        +--> key B
        |
        v
SetSecurityInfo(B,keyA,keyB)
```

The MHI2 replica must translate structure offsets and negotiated-value representation separately. Q offsets must not be copied blindly.

## Socket / worker path

Q opens a second listener through its seven-argument `ServerSocketOpen` ABI, publishes the resulting port as the 111 `dataPort`, and accepts the connection in a dedicated worker. The worker is created by `pthread_create` at cluster address `0x4128`, with start routine `0x38fc`. Inside that worker, `SocketAccept` is directly loaded from API slot `0x10328` and called at `0x395c`; `AirPlayReceiverSessionScreen_StartSession` is directly loaded from slot `0x101e0` and called at `0x3b60` after the accept path succeeds.

The direct Q worker trace is closed: `NetSocket_CreateWithNative` is called at `0x3a70` through its resolved API slot; `StartSession(B,arg2)` is called at `0x3b60`; `ProcessFrames(B,NetSocket,[context+0x1c8])` is called at `0x3bbc`.

MHI2 uses the same seven-argument AAPCS shape, but the native values differ. Static tracing identifies the stock `0x23380` screen/media listener as the MHI2 translation target: `arg1=session+0x51 byte`, TCP (`1,6`), `arg4=0xFFFFE872`, `arg6=0xFFFD4000`, with independent arg5/arg7 outputs. The `0x23C48` listener is the RAOP event channel (`-5001`) and is not the screen listener.

## Decoder translation

Q resolves an OMX video lifecycle. MHI2 stock decoding ends in `NvssVideoImpl::decode`.

This is a platform translation boundary:

```
Q:   OMX lifecycle
MHI2: NvSS lifecycle
```

Only the actual decoder primitive should be translated; surrounding Q object/offset assumptions require separate MHI2 evidence.

## Output architecture

The working Q package contains a separate executable:

```
/eso/bin/apps/mhi2q-aa-cluster-display
```

The Q CarPlay cluster imports `spawnl` and has a direct launch edge using the embedded `--live-carplay` argument. The helper's live mode consumes the local H.264 stream and uses QNX Screen/display APIs plus a DMDT route.

Thus the functional Q output architecture is:

```
111 SessionScreen
   -> secondary transport
   -> H.264/decoder path
   -> local producer/loopback transport
   -> mhi2q-aa-cluster-display --live-carplay
   -> QNX Screen window
   -> Q-specific DMDT/VC route
```

The Q geometry, DMDT constants, and helper internals are not hardware-independent. For MU0678, the recovered logical KOMBI target is `DISPLAYABLE_KOMBI_MAP_VIEW` (1440x455), but physical panel mapping remains a runtime/hardware fact.

## What this proves for the MHI2 replica

The functional specification is not “replace libairplay” and not “make stock 110 StartSession accept 111”.

It is:

```
preserve 110
    |
stock AirPlay + DIO loader boundary
    |
detect existing 111 stream
    |
create independent SessionScreen B
    |
Q-equivalent Setup / time / security
    |
secondary listener + 111 dataPort
    |
accept + NetSocket
    |
post-accept StartSession(B,...)
    |
ProcessFrames(B, NetSocket, Q-context)
    |
secondary decode/output sink
```

The current MHI2 work has statically established many of these operations. The implementation must reproduce them at MU0678-native ABI/object boundaries.

## Evidence boundary

### Q-proven

- the working installer preserves stock `libairplay.so`;
- the DIO loader dynamically links against stock AirPlay;
- 110 and 111 are separately identified;
- B is a second SessionScreen;
- B receives UpdateState and Setup;
- Setup receives the real CreateResult as argument 3;
- B receives the time synchronizer before security completion;
- B security uses the stock AES/SHA512 helper;
- a second listener is opened and its port is advertised as 111 dataPort;
- the secondary socket has a dedicated pthread worker;
- `SocketAccept` is directly called by that worker;
- `NetSocket_CreateWithNative` is directly called at `0x3a70`;
- `StartSession(B,...)` is directly called at `0x3b60` after accept/wrapping;
- `ProcessFrames(B, NetSocket, [context+0x1c8])` is directly called at `0x3bbc`.
- the Q package contains a dedicated live-CarPlay display helper;
- Q has executable native Screen creation/registration symbols and the SessionScreen-created delegate callback.

### MHI2-specific and separate

- MU0678 native offsets, negotiated-value fields, and delegate bookkeeping;
- the exact external dispatcher equivalent to Q's session-screen-created callback;
- the exact MU0678 third-argument/CreateResult construction for secondary ScreenSetup, if not already preserved by the adapter;
- MU0678 ScreenStream/NvSS implementation;
- successful end-to-end live 111 decode/display.

These must be established independently on MHI2.

### Critical non-reference artifacts

The experimental MU0678 `libairplay_*111*` variants and `libairplay_patched_v6.so` are **not** the functional reference. The supplied original MU0678 patch remains useful only as evidence of a possible binary-injection mechanism; it must not be described as the working Q implementation.

