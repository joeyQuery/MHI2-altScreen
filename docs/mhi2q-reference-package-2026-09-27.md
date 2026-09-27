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

## Important correction: StartSession is post-accept

Q dynamically resolves `AirPlayReceiverSessionScreen_StartSession`, but the **initial type-111 SETUP handler does not call it**. The separate secondary-socket worker does call it after `SocketAccept` and `NetSocket_CreateWithNative`.

The worker loads the StartSession argument 2 through:

```text
worker context
  -> [r9 + 0x0c]
  -> [that object + 0x1d0]
  -> StartSession(B, arg2)
```

The binary proves this object/offset chain. The exact C++ class identity of the `+0x1d0` owner is not recovered from the stripped Q cluster alone. Independently, MU0678 StartSession analysis proves that its corresponding argument #2 is configuration/context input, not a hidden native Screen-B selector. Therefore the Q `+0x1d0` field must not be renamed `ScreenB`; it is the Q-side StartSession context object.

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

The MHI2 replica must translate the structure offsets and negotiated-value representation separately. Q offsets must not be copied blindly.

## Socket path

Q opens a second listener through its seven-argument `ServerSocketOpen` ABI, publishes the resulting port as the 111 `dataPort`, accepts the connection, wraps the native socket with `NetSocket_CreateWithNative`, and passes that socket into `ProcessFrames`.

MHI2 uses the same seven-argument AAPCS shape, but the native values differ. Static tracing identifies the stock `0x23380` screen/media listener as the MHI2 translation target: `arg1=session+0x51 byte`, TCP (`1,6`), `arg4=0xFFFFE872`, `arg6=0xFFFD4000`, with independent arg5/arg7 outputs. The `0x23C48` listener is the RAOP event channel (`-5001`) and is not the screen listener.

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

- MU0678 native offsets, negotiated-value fields, and delegate bookkeeping.
- The exact external dispatcher equivalent to Q's session-screen-created callback.
- The exact MU0678 third-argument/CreateResult construction for the secondary ScreenSetup, if not already preserved by the adapter.
- MU0678 ScreenStream/NvSS implementation.
- Successful end-to-end live 111 decode/display.

These must be established independently on MHI2.
