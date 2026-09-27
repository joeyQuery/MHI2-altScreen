# MHI2Q working parallel-111 lifecycle — 2026-09-27

This document is derived directly from the supplied working `install.zip`, specifically `libmhi2qcarplaycluster.so` SHA-256:

`e3fa80fb3326bd470df6a1dc2c6866148b67e8f5d19bb1d98c97f4662f6d76c9`

It supersedes any lifecycle interpretation based on experimental `libairplay.so` patches.

## 1. SETUP handler owns discovery and B construction

The relevant function begins at Q cluster offset `0x3c60`.

The stream collection is enumerated beginning at `0x3ccc`.

The discriminator handling is explicit:

```
0x3ccc  cmp r1,#0
0x3cd0  cmpeq r0,#110
0x3cd4  moveq r5,#1

...

0x3d18  cmp r1,#0
0x3d1c  cmpeq r0,#111
0x3d20  bne  0x3ccc
0x3d28  str  r6,[sp,#0x14]
```

So the handler:

- recognises the existing 110 stream;
- separately retains the 111 stream object;
- requires a 111 stream object before continuing.

This is direct binary evidence, not an inferred protocol model.

## 2. B SessionScreen is created inside that SETUP path

The same function subsequently reaches the dynamically resolved `AirPlayReceiverSessionScreen_Create` pointer.

The Q resolver imports the symbol explicitly and stores the resolved pointer in the cluster's function-pointer table.

The working setup then performs the B-object setup/update/security sequence before publishing the secondary data endpoint.

The relevant operation ordering is:

```
find 110 / 111
    |
    +--> retain stream111
    |
    +--> Create SessionScreen B
    |
    +--> UpdateState(B,1)
    |
    +--> ScreenSetup(B,...)
    |
    +--> time/security preparation
    |
    +--> DeriveAESKeySHA512ForScreen(...)
    |
    +--> SetSecurityInfo(B,...)
    |
    +--> ServerSocketOpen(...)
    |
    +--> advertise type=111 + dataPort
```

The exact Q instructions at `0x3f54` onward resolve and invoke the Create/UpdateState/Setup function-pointer slots; `0x402c`, `0x405c`, and `0x40b0` reach the corresponding security/listener primitives.

## 3. The 111 SETUP handler does NOT itself call StartSession/ProcessFrames

This distinction is important.

There is no direct call through the resolved `AirPlayReceiverSessionScreen_StartSession` or `AirPlayReceiverSessionScreen_ProcessFrames` slots in the `0x3c60` SETUP handler before it returns to the surrounding session path.

Therefore statements such as "the SETUP function immediately calls StartSession" are incorrect.

However, that does **not** mean the working Q implementation never calls StartSession.

## 4. A separate secondary-socket worker performs StartSession and ProcessFrames

The working cluster contains a separate function beginning at `0x3900`.

Its operation is structurally distinct from the SETUP handler.

### Secondary listener accept

At `0x3940-0x395c` it calls the resolved `SocketAccept` function with the secondary listener context.

### Native socket wrapping

At `0x3a5c-0x3a70` it calls the resolved `NetSocket_CreateWithNative` function.

### StartSession

At `0x3b38-0x3b60` the worker:

- loads the secondary SessionScreen context;
- loads an object at `[r6+0x1d0]`;
- sets the worker state flag;
- calls the resolved `AirPlayReceiverSessionScreen_StartSession` pointer.

The decisive call is:

```
0x3b44  mov r0,r5
0x3b4c  ldr r1,[r6,#0x1d0]
0x3b5c  ldr r3,[resolved StartSession slot]
0x3b60  blx r3
```

Thus StartSession is **post-accept**, not part of the initial 111 SETUP block.

### ProcessFrames

After StartSession succeeds, the worker reaches:

```
0x3ba0  ... resolved ProcessFrames slot
0x3ba4  mov r0,r5
0x3ba8  ldr r2,[r6,#0x1c8]
0x3bb0  ldr r1,[sp,#0x8]
0x3bb8  ldr r3,[resolved ProcessFrames slot]
0x3bbc  blx r3
```

So the actual working Q sequence is:

```
SETUP
  -> retain 111
  -> Create B
  -> configure/security
  -> open secondary listener
  -> advertise 111/dataPort

secondary worker
  -> SocketAccept
  -> NetSocket_CreateWithNative
  -> StartSession(B, screen-context)
  -> ProcessFrames(B, socket/context)
```

## 5. Consequence for MHI2

The replica must preserve this lifecycle split.

Do **not** put the complete secondary StartSession/ProcessFrames operation into the initial SETUP hook merely because the Q package resolves those symbols.

The correct implementation boundary is:

```
AirPlayReceiverSessionSetup
    |
    +--> construct/configure B
    +--> publish 111 listener/descriptor
    |
    v
secondary connection worker
    |
    +--> accept
    +--> native socket wrapper
    +--> StartSession(B, correct Screen object)
    +--> ProcessFrames(B, socket)
```

The critical unresolved MHI2-specific operation remains the correct native Screen object passed as StartSession argument 2. Q proves that this object is supplied separately at worker time; it must not be confused with the AirPlay SessionScreen itself.

## 6. Important correction to previous repository wording

The accurate statement is:

> The Q **type-111 SETUP handler** does not call StartSession directly. The working Q **overall type-111 implementation does call StartSession later from its secondary-socket worker, after SocketAccept and NetSocket_CreateWithNative, and then calls ProcessFrames.**

This resolves the apparent contradiction between the Q symbol import list and the earlier observation that the setup block itself contained no StartSession call.
