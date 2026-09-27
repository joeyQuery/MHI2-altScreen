# MU0678 111 implementation blueprint — 2026-09-27

This document is the implementation-ready translation of the audited MHI2Q reference onto MU0678. It is deliberately a specification, not a claim that a flashable MU0678 binary has already been patched.

## Non-negotiable invariant

The stock 110 path remains unchanged.

Do not modify `ScreenCopyMain()`, do not replace the stock 110 `SessionScreen`, and do not reuse Q absolute addresses, Q socket constants, Q OMX object layouts, or Q 1440x540 geometry.

## Functional lifecycle to implement

The MU0678 implementation must add a parallel 111 path at the AirPlay session/SETUP boundary:

```
SETUP streams[]
  -> find stream type 111
  -> retain 111 descriptor
  -> Create SessionScreen B
  -> UpdateState(B, 1)
  -> ScreenSetup(B, stream111, MU0678-native ABI)
  -> SetTimeSynchronizer(B, ...)
  -> obtain 111 negotiated 64-bit value
  -> DeriveAESKeySHA512ForScreen(session+0x1c0, 16, negotiated64)
  -> SetSecurityInfo(B, derivedA, derivedB)
  -> open secondary screen/media listener
  -> publish {type:111,dataPort:N}
  -> accept secondary connection
  -> accept secondary connection
  -> NetSocket_CreateWithNative(acceptedFd)
  -> post-accept StartSession(B, MU0678 context)
  -> ProcessFrames(B, NetSocket, Q-context)
  -> MU0678-native secondary frame/decode path
  -> dedicated 111 output sink
```

The Q reference proves that the type-111 setup block itself does not call `StartSession`; the start occurs after the secondary socket is accepted. MU0678 must preserve that ordering.

## MU0678-native security sequence

The stock 110 path establishes the exact key derivation ABI:

- `session + 0x1c0): 16-byte session security material.
- negotiated 64-bit value: read by `CFDictionaryGetInt64` from the stream's negotiated field at `stream + 0x284`.
- call `AirPlay_DeriveAESKeySHA512ForScreen(session+0x1c0, 16, negotiated64)`.
- pass both 16-byte outputs to `AirPlayReceiverSessionScreen_SetSecurityInfo(B,...)`.

Do not invent a second security ABI for 111.

MU0678 `SetTimeSynchronizer` is:

```
memcpy(screen + 0x20, p, 12);
```

The additional arguments used by the caller are ignored by this MU0678 implementation.

## MU0678 secondary listener

The native `ServerSocketOpen` ABI is seven-argument AAPCS. The stock screen/media listener at `AirPlayReceiverSessionSetup` callsite `0x23380` provides the MU0678-native recipe:

- arg1 = `*(uint8_t *)(session+0x51)`
- arg2 = `1`
- arg3 = `6`
- arg4 = `0xFFFFE872`
- arg5 = fresh bound-result storage
- arg6 = `0xFFFD4000`
- arg7 = fresh socket-fd storage

These values are not interchangeable with Q's `session+0x55, 0x17D4, -1` recipe.

The secondary listener's resulting data port is the value returned in the 111 response descriptor.

## ScreenStream / decoder boundary

MU0678 stock frame processing is:

```
ProcessFrames
  -> ScreenStreamProcessData
  -> _ScreenStreamDecodeAll
  -> NvssVideoImpl::decode
```

`ScreenStream` stores its stream type at `+0x94`. The new 111 stream must remain independently identified as 111.

The decoder must be a fresh MU0678 `NvssVideoImpl` instance. Do not port Q's `OmxVideoImpl` layout.

`NvssVideoImpl` has a per-instance handle at `+0x50`, so independent decoder state is structurally supported.

## Native display boundary

Stock MU0678 `StartSession` creates a ScreenStream and clones properties from the native main Screen. Its second argument is context/configuration, not a native Screen selector.

Therefore B cannot be routed to the VC merely by supplying a different `StartSession` argument.

The safe functional translation of the Q output boundary is a separate 111 H.264 producer/output sink:

```
MU0678 SessionScreen B
  -> ProcessFrames
  -> independent NvSS decode
  -> H.264/frame transport
  -> dedicated QNX Screen/DMDT sink
  -> MU0678 VC/KOMBI route
```

The recovered MU0678 logical target is `DISPLAYABLE_KOMBI_MAP_VIEW` (ID 33), with 1440x455 geometry. The physical panel mapping remains a hardware/runtime fact and must not be inferred from the displayable ID alone.

Do not copy Q's 1440x540 geometry.

## Teardown

Preserve the Q-proven order:

1. original AirPlay `SessionTearDown`
2. close secondary listener
3. join secondary accept/worker thread
4. `ScreenStreamStop(streamB)`
5. `StopSession(B)`
6. `ScreenDeregister(vc_screen)` when a dedicated native Screen was actually registered
7. clear duplicate-state references

Teardown must never deregister or destroy the stock 110 Screen.

## What is now statically closed

- 111 detection and descriptor retention.
- independent SessionScreen B lifecycle.
- UpdateState(B,1).
- MU0678 ScreenSetup ABI and the fact that its third argument is overwritten internally.
- MU0678 time synchronizer behaviour.
- MU0678 AES derivation inputs and SetSecurityInfo boundary.
- MU0678 seven-argument listener ABI and stock screen/media listener recipe.
- Q 111/dataPort response.
- secondary accept and post-accept worker boundary.
- post-accept StartSession ordering.
- Q direct `NetSocket_CreateWithNative` and `ProcessFrames` worker callsites.
- MU0678 stock ProcessFrames/NvSS chain as the native decoder translation.
- Q's dedicated live-CarPlay output transport: accepted helper socket -> H.264 loopback -> 127.0.0.1:15557 -> QNX Screen helper.
- Q helper launch via `spawnl(...,"--live-carplay",...)`.
- Q helper's direct Screen/display API boundary and DMDT VC-routing invocation.

## Still runtime-dependent

A complete implementation cannot honestly be called finished until hardware demonstrates:

- the iPhone actually requests 111 after MU0678 advertises `altScreen`;
- the secondary listener receives the 111 connection;
- B decodes continuously without disturbing 110;
- the dedicated output sink displays the decoded frames on the intended VC/KOMBI surface;
- simultaneous 110 + 111 remains stable through teardown/reconnect.

Those are validation gates, not unresolved architecture questions.

## Patch construction rule

Where the repository has only reverse-engineered addresses and no compilable MU0678 source, implementation work must be represented as exact hook/ABI specifications until the corresponding executable bytes and relocation-safe code caveats are available. A document claiming a flashable binary without those bytes would be misleading.
