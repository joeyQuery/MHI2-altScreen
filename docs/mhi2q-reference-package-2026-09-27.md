# MHI2Q AltScreen reference package — audited 2026-09-27

This document records the MHI2Q implementation evidence used as the functional reference for the MHI2 replica.

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
          +-- ProcessFrames
          +-- secondary ScreenStream / decoder integration
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
8. call `AirPlay_DeriveAESKeySHA512ForScreen(session+0x1c0,16,negotiated64)`;
9. call `SetSecurityInfo(B,keyA,keyB)`;
10. open the secondary listener;
11. advertise `{type:111,dataPort:N}`;
12. accept the secondary connection;
13. wrap it with `NetSocket_CreateWithNative`;
14. call `ProcessFrames(B,socket)`;
15. integrate the resulting secondary stream with DIO/decoder/output;
16. tear the secondary path down alongside the main path.

## Important correction: StartSession

Q dynamically resolves `AirPlayReceiverSessionScreen_StartSession`, but the proven type-111 setup path does **not** call it.

Therefore:

- do not add an artificial `StartSession(B)` merely because the symbol exists;
- the replica must follow the observed Q path rather than assumed symmetry with stock MHI2 110.

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

The MHI2 replica must translate the structure offsets and negotiated-value representation separately. Q offsets must not be copied blindly.

## Socket path

Q opens a second listener through its seven-argument `ServerSocketOpen` ABI, publishes the resulting port as the 111 `dataPort`, accepts the connection, wraps the native socket with `NetSocket_CreateWithNative`, and passes that socket into `ProcessFrames`.

MHI2's stock `ServerSocketOpen` ABI differs. Therefore the socket parameters must be reconstructed on MHI2 rather than copied literally from Q.

## Decoder translation

Q resolves an OMX video lifecycle. MHI2 stock decoding ends in `NvssVideoImpl::decode`.

This is a platform translation boundary:

```
Q:   OMX lifecycle
MHI2: NvSS lifecycle
```

Only the actual decoder primitive should be translated; surrounding Q object/offset assumptions require separate MHI2 evidence.

## What this proves for the MHI2 replica

The functional specification is not “make stock 110 StartSession accept 111”.

It is:

```
preserve 110
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
ProcessFrames(B,...)
    |
secondary decode/display path
```

The current MHI2 implementation already contains many of these operations. The remaining work is to make each operation match the Q behaviour at the exact ABI/object boundary, without disturbing 110.

## Evidence boundary

### Q-proven

- 110 and 111 are separately identified.
- B is a second SessionScreen.
- B receives UpdateState and Setup.
- Setup receives the real CreateResult as argument 3.
- B receives the time synchronizer before security completion.
- B security uses the stock AES/SHA512 helper.
- A second listener is opened and its port is advertised as 111 dataPort.
- The secondary socket is accepted and wrapped.
- ProcessFrames is called for B.
- Q resolves StartSession, but the traced type-111 path does not call it.
- Q has executable native Screen creation/registration symbols and the SessionScreen-created delegate callback.

### MHI2-specific and separate

- MU0678 native offsets, socket arguments, negotiated-value fields, and delegate bookkeeping.
- MU0678 ScreenStream/NvSS implementation.
- The exact external dispatcher equivalent to Q's session-screen-created callback.
- Successful end-to-end live 111 decode/display.

These must be established independently on MHI2.
