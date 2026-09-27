# MU0678 type-111 implementation trace — 2026-09-27

## Authoritative inputs

- Reference package: install.zip
- SHA-256: 69a186d31b320b922e536581fb2f3fe4bdce2517c7dae34fbd8ae6b0dd9bd60b
- MU0678 libairplay.so: the verified target image in the repository dump.
- MU0678 dio_manager: the verified target DIO executable.

## The missing SETUP boundary is now proven

The MU0678 exported AirPlayReceiverSessionSetup is at 0x22fd8.

At entry it saves:

- r0 (session) at sp+0x50 and keeps it in r6.
- r1 (stream array) at sp+0x50.
- r2 (setup result) at sp+0x68.

Inside the stream loop:

- CFArrayGetTypedValueAtIndex at PLT 0x170d8 returns the current stream into r9.
- CFDictionaryGetInt64 at PLT 0x15b60 reads the stream type using key base r10+392.
- 0x2325c compares that type against 110.
- 0x23260 is the exact bne 0x231f0 boundary.
- At 0x232d4, the same CFDictionaryGetInt64 reads the stream's negotiated 64-bit value using key r10+644.

Therefore the exact MU hook ABI at 0x23260 is reconstructed without guessing:

    session       = r6
    currentStream = r9
    setupResult   = [sp+0x68]
    keyBase       = r10

The implementation installs an executable veneer at runtime. The veneer tests streamType == 111, calls mu0678_interpose_setup111(session,currentStream,setupResult,keyBase), restores the complete register set, and continues at the stock 0x231f0 path. The stock 110 path at 0x23264 is not replaced.

## Exact MU 111 response construction

The stock MU 110 path proves the response-stream recipe.

CFDictionaryCreateMutable is the PLT entry used at 0x2328c.

Its callback-table addresses derive from the MU setup base:

- setup base r4 = libairplay + 0xa9418
- key callbacks: *(libairplay + 0xaa3b8)
- value callbacks: *(libairplay + 0xaa390)

The response-stream dictionary then receives:

- stream-type key: libairplay + 0x941dc
- stream-data-port key: libairplay + 0x942c4

The stock path performs CFDictionarySetInt64(response,keyType,110,0), CFDictionarySetInt64(response,keyPort,dataPort,0), then _AddResponseStream(response,setupResult). The 111 hook performs the same operation with 111 and the actual port returned by the MU listener.

_AddResponseStream is local MU code at 0x22f34.

## Exact MU secondary listener

The stock MU media listener at AirPlayReceiverSessionSetup+0x3a8 (0x23380) proves:

    owner = *(uint8_t *)(session + 0x51)
    arg2  = 1
    arg3  = 6
    arg4  = 0xffffe872
    arg5  = data-port output
    arg6  = 0xfffd4000
    arg7  = listener-fd output

The 111 implementation reuses this exact MU ABI. No Q port number is transplanted.

## Exact MU secondary SessionScreen recipe

The cluster performs the proven sequence:

    SessionScreen_Create(session, 0)
    UpdateState(B, 1)
    Screen_Setup(B, stream111, setupResult)
    AirPlay_DeriveAESKeySHA512ForScreen(session+0x1c8, 16, negotiated64)
    SetSecurityInfo(B, keyA, keyB)

MU0678's SessionUUID, ClientDeviceID, receive/auth/NTP end-time setters are no-ops in this image, so no fabricated state writes are needed.

## Exact MU worker topology

    SocketAccept(listener, 10, &nativeFd, 0)
    NetSocket_CreateWithNative(nativeFd)
    StartSession(B)
    ctx  = *(session + 0x0c)
    arg3 = *(ctx + 0xc8)
    ProcessFrames(B, netSocket, arg3)
    NetSocket_Delete(netSocket)

The +0xc8 context offset is deliberately MU-native; Q's +0x1c8 is not copied.

## Exact MU decoder association

ScreenStreamCreate is at 0x7f544.

AirPlayReceiverSessionScreen_StartSession calls its PLT entry at 0x17d2c, whose GOT slot is libairplay + 0xaa2d8.

ScreenStreamCreate stores the MU video implementation at ScreenStream + 0x48.

The implementation temporarily marks the secondary StartSession(B) operation as capture-active, wraps ScreenStreamCreate, and records exactly that ScreenStream+0x48 decoder object.

## Exact MU raw-frame tee point

NvssVideoImpl::decode is at 0x7faf0.

Inside it, the raw decode call occurs at 0x7fb68. At that point r0 is the video implementation, r1 is a pointer to the input-buffer pointer, and the caller's original access-unit length is at [sp+0x14].

The implementation replaces only this BL with a runtime veneer. The veneer reads the length before changing the frame, calls the tee only when r0 equals the captured secondary decoder, restores all original registers, and jumps to the original NvSSVideoDecode PLT entry with the original continuation in lr.

Primary 110 decoder traffic is therefore not copied merely because 111 is active.

## AAF1 output

The existing output implementation preserves the Q framing:

    00-03  AAF1
    04     02
    05     stream mode
    06     flags
    07     20
    08-0B  object+0x18
    0C-0F  object+0x04
    10-13  object+0x0C
    14-17  object+0x08
    18-1B  object+0x14
    1C-1F  object+0x10

The MU decoder tee currently publishes access units through this framing with zero metadata fields rather than inventing MU object offsets that have not been proven equivalent to Q's OmxVideoImpl object.

That is intentional: raw H.264 duplication is statically proven; the six AAF1 metadata fields remain a display-helper compatibility/runtime validation point.

## Installation safety

- AirPlayReceiverSessionSetup address derives the MU libairplay base.
- ScreenStreamCreate GOT is guarded against both lazy-PLT and already-resolved states.
- SETUP branch old word must be exactly 0x1affffe2.
- NvSS decode call old word must be exactly 0xebfe6021.
- The executable veneer is allocated within the ARM branch range and instruction cache is flushed.
- No Q absolute code address is copied into MU.

## Remaining gates

Static architecture is now closed through:

111 SETUP -> response port -> secondary listener -> SessionScreen B -> StartSession -> secondary ScreenStream -> secondary NvssVideoImpl -> raw access-unit tee -> AAF1 sink.

Genuinely hardware-dependent gates remain:

1. Confirm iPhone opens the advertised 111 data port.
2. Confirm the second listener receives that connection while 110 remains alive.
3. Confirm B ProcessFrames accepts and decrypts the 111 stream.
4. Confirm NvssVideoImpl receives only the secondary stream through the captured decoder.
5. Confirm mhi2q-aa-cluster-display --live-carplay accepts the MU-produced AAF1 metadata and renders it to the intended DMDT route.
6. Confirm simultaneous MMI 110 + VC 111 display.
7. Confirm disconnect/reconnect and teardown under repeated sessions.

No static evidence currently justifies claiming those hardware outcomes.
