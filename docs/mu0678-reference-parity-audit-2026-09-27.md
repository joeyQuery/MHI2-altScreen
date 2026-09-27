# MU0678 reference parity audit — 2026-09-27

## Rule

The supplied `install.zip` is the working implementation specification.

A difference is permitted only when the reference mechanism is tied to an MHI2Q-only object, ABI, binary layout, or service that MU0678 demonstrably does not have. A difference is **not** permitted merely because an implementation is simpler.

## Reference package facts

The working package contains these CarPlay-relevant artifacts:

- `dio_manager.mhi2q_carplay_loader`
- `libmhi2qcarplaycluster.so`
- `mhi2q-aa-cluster-display`
- `dio_manager.json`

The reference CarPlay cluster is ELF32 ARM/QNX and has:

- SONAME `libmhi2qcarplaycluster.so`
- `libsocket.so.3` as its explicit shared-library dependency
- dynamic resolution of the AirPlay/Screen/CF surface
- QNX `mprotect` guarded code interposition
- exact helper path `/eso/bin/apps/mhi2q-aa-cluster-display`
- helper argument `--live-carplay`
- 111 metadata including `altScreen`, `streamConnectionID`, `dataPort`, `features`, and `displays`

The reference cluster dynamically resolves, among others:

```
AirPlayReceiverSessionSetup
AirPlayReceiverSessionTearDown
AirPlayCopyServerInfo
AirPlayReceiverSessionSendCommand
AirPlayReceiverSessionForceKeyFrame
ScreenStreamCreate
AirPlayReceiverSessionScreen_Create
AirPlayReceiverSessionScreen_Delete
AirPlayReceiverSessionScreen_Setup
AirPlayReceiverSessionScreen_SetTimeSynchronizer
AirPlayReceiverSessionScreen_SetSecurityInfo
AirPlayReceiverSessionScreen_UpdateState
AirPlayReceiverSessionScreen_StartSession
AirPlayReceiverSessionScreen_ProcessFrames
AirPlayReceiverSessionScreen_StopSession
AirPlay_DeriveAESKeySHA512ForScreen
ServerSocketOpen
SocketAccept
NetSocket_CreateWithNative
NetSocket_Delete
CFDictionaryGetInt64
CFDictionarySetInt64
CFDictionaryCreateMutable
CFArray...
```

The Q resolver opens the current process with `dlopen(NULL, 0x80000100)` and resolves the symbols with `dlsym`. The MU replica now follows that loader semantic rather than opening a separate library handle.

## Directly reproducible reference behaviour

These must remain identical in function:

1. secondary type 111 detection;
2. second SessionScreen creation;
3. `UpdateState(B,1)`;
4. `Setup(B,111,CreateResult)`;
5. time synchronizer before AES/security;
6. AES derivation;
7. `SetSecurityInfo`;
8. second media listener;
9. response containing type 111 and its data port;
10. post-accept `NetSocket_CreateWithNative`;
11. post-accept `StartSession(B)`;
12. `ProcessFrames(B,...)`;
13. secondary ScreenStream/NvSS capture;
14. Q-compatible live-CarPlay helper;
15. 32-byte AAF1 framing;
16. secondary teardown;
17. stock 110 remains untouched.

The current implementation contains these operations.

## Proven MU0678 translations

These are not Q deviations; they are target-image translations:

### Security material

MU0678 stock SETUP derives the secondary AES material from:

```
session + 0x1c8
length = 16
negotiated 64-bit stream value
```

The repository documentation has been corrected to `+0x1c8`.

### Secondary listener

MU0678 stock caller at `libairplay + 0x23380` proves:

```
owner = *(uint8_t *)(session + 0x51)
arg2 = 1
arg3 = 6
arg4 = 0xffffe872
arg5 = bound-port output
arg6 = 0xfffd4000
arg7 = listener-fd output
```

Those values replace the Q-specific listener state because the MU0678 listener ABI is different.

### NvSS

Q resolves OMX lifecycle methods; MU0678 has NvSS. The actual MU0678 secondary decoder primitive is therefore translated to `NvssVideoImpl::decode` at the recovered raw call boundary `+0x7fb68`. This is a genuine architecture-specific translation.

### DIO executable

The Q package's `dio_manager.mhi2q_carplay_loader` cannot be copied to MU0678. It is a different executable with different symbols, offsets, dependencies, and callbacks.

The package script therefore no longer substitutes that Q executable. It requires the target MU0678 `dio_manager` as an input and stages it unchanged.

This is an architectural requirement, not a convenience difference.

## Reference mechanisms still requiring exact MU0678 translation

These are **not** allowed to be declared “optional” yet:

### DIO load/interposition boundary

The reference Q loader has an explicit NEEDED dependency on `libmhi2qcarplaycluster.so` and contains the Q DIO callback/interposition machinery.

MU0678's native `dio_manager` does not currently contain that NEEDED dependency.

The current source's shared-library constructor is only effective after the cluster has actually been loaded. Therefore the loader boundary itself remains an implementation item and cannot be considered solved by the constructor.

### /info

The MU0678 target has now been translated at the exact DIO callback boundary. Its
s_serverCopyPropertyCallback is a R_ARM_GLOB_DAT at 0x19d9d8, and the
cluster derives the DIO image base from the exported callback symbol before
guardedly replacing that slot.

The MU hook reproduces the reference's first /info mechanism:

1. call AirPlayCopyServerInfo(server);
2. CFPropertyListCreateDeepCopy(NULL, info, 2);
3. add altScreen = 1 with CFDictionarySetInt64;
4. return the copied dictionary.

The reference also adds additional display/stream metadata
(streamConnectionID, dataPort, features, displays, and display geometry);
those fields remain open until their exact value construction is statically
mapped rather than guessed.

### AirPlay teardown

The MU0678 s_sessionFinalizedCallback is a R_ARM_GLOB_DAT at 0x19da00.
The cluster now replaces that exact slot with a wrapper that performs the
secondary stop/delete path before invoking the original callback. This closes
the previously missing target-side teardown boundary.

### SendCommand / ForceKeyFrame

The reference cluster resolves `AirPlayReceiverSessionSendCommand` and `AirPlayReceiverSessionForceKeyFrame`. These are part of the working cluster's runtime surface. The current MU implementation does not yet prove an equivalent target-side use/interposition.

They remain open until static tracing establishes whether they are CarPlay-111 functional dependencies or package-wide ancillary functionality.

## Output helper

The helper is directly copied from the supplied package by SHA and is not rebuilt:

```
0ad556112e427745da99e057affde5b18195db875398d2111cb131f790b7d65e
```

No architecture translation is made here.

## Explicitly rejected shortcuts

The following are not acceptable parity claims:

- “the constructor is enough” before proving the MU DIO actually loads the cluster;
- “/info is irrelevant” without tracing the reference metadata path;
- “teardown is equivalent” merely because an internal stop function exists;
- copying Q DIO absolute offsets into MU0678;
- copying Q SessionScreen/security offsets without MU0678 proof;
- replacing the reference helper with a newly written viewer;
- declaring runtime success without 111 + simultaneous 110 hardware evidence.

## Current parity status

### Closed

- exact reference helper;
- AAF1 framing structure;
- MU0678 SessionScreen B lifecycle;
- MU0678 security input;
- MU0678 time-sync ordering;
- MU0678 secondary listener ABI;
- MU0678 post-accept StartSession/ProcessFrames boundary;
- secondary ScreenStream/NvSS capture boundary;
- stock 110 preservation;
- QNX guarded patch primitive semantics;
- Q/MU0678 distinction for Q-owned BSS offsets;
- target-specific package identity.

### Open and required

- actual MU0678 DIO loader/load boundary;
- exact MU0678 `/info`/AltScreen advertisement mechanism;
- exact MU0678 teardown interception;
- exact MU0678 command/keyframe dependencies if proven part of 111;
- final display routing and runtime validation.

No open item is being classified as an “architecture difference” merely to make the parity report look complete.
