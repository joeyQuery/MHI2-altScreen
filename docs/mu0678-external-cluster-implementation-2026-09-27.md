# MU0678 external cluster implementation — 2026-09-27

## Target

Reproduce the authoritative MHI2Q `install.zip` architecture on MU0678 without replacing the stock `libairplay.so` file.

The Q package's actual structure is:

```
dio_manager.mhi2q_carplay_loader
        |
        +-- NEEDED: libmhi2qcarplaycluster.so
        |
        +-- stock libairplay.so
        |
        +-- DIO callback symbols
```

The cluster is loaded into the DIO process and then resolves/interposes live functions.

## New static conclusion

The MU0678 implementation does **not** need to be a replacement `libairplay.so`.

The correct implementation boundary is:

```
stock MU0678 dio_manager
        |
        +-- existing AirPlay/DIO callbacks
        |
        +-- MU0678 CarPlay cluster shared object
                |
                +-- runtime interposition
                +-- 111 SessionScreen lifecycle
                +-- MU0678 NvSS decoder path
                +-- dedicated output sink
```

A DIO-manager replacement remains possible, but it is not intrinsically required by the Q cluster architecture. The authoritative Q package replaces the executable because its product was built for Q's DIO dependency graph; the cluster itself dynamically resolves its host APIs.

## MU0678 host symbols

The stock MU0678 DIO ELF exports the following relevant callback functions:

| Function | MU0678 address |
|---|---:|
| `s_sessionScreenCreatedCallback` | `0x1362d0` |
| `s_sessionCreatedCallback` | `0x138fb4` |
| `s_sessionModesChangedCallback` | `0x139268` |
| `s_sessionFinalizedCallback` | `0x139424` |
| `s_serverCopyPropertyCallback` | `0x139a88` |
| `s_sessionCopyProperty` | `0x138ef0` |
| `s_sessionNotifyProperty` | `0x139?` — address must be resolved from the exact MU0678 symbol table before implementation |

The first five above are directly established from the MU0678 dynamic symbol table.

## Q host-symbol anchors

The authoritative Q loader exports:

| Function | Q address |
|---|---:|
| `s_sessionScreenCreatedCallback` | `0x154cdc` |
| `s_sessionCreatedCallback` | `0x1579f4` |
| `s_sessionModesChangedCallback` | `0x157f1c` |
| `s_sessionFinalizedCallback` | `0x1580d8` |
| `s_serverCopyPropertyCallback` | `0x158680` |

The Q cluster resolves the `s_serverCopyPropertyCallback` symbol by name and uses the resulting address as an executable-layout anchor.

Relative callback offsets are therefore:

### Q

```
serverCopyProperty       0x158680
sessionCreated           0x1579f4   = -0x68c
sessionModesChanged      0x157f1c   = -0x764
sessionFinalized         0x1580d8   = -0x5a8
sessionScreenCreated     0x154cdc   = -0x39a4
```

### MU0678

```
serverCopyProperty       0x139a88
sessionCreated           0x138fb4   = -0x0ad4
sessionModesChanged      0x139268   = +0x07e0
sessionFinalized         0x139424   = +0x039c
sessionScreenCreated     0x1362d0   = -0x2db8
```

**Do not copy the Q relative offsets.** These values demonstrate exactly what must be translated when implementing the MU0678 cluster.

## Q runtime patch primitive

The Q cluster has a dedicated 4-byte patch primitive at approximately `0x47f8`.

Its verified behavior is:

1. read the current 32-bit word at the target;
2. compare it against one or more expected old values;
3. refuse the patch if the instruction/value is not recognised;
4. obtain the system page size;
5. align the target to the page boundary;
6. call `mprotect`;
7. write the replacement 32-bit word;
8. issue a data-memory barrier;
9. return success/failure.

This is a **runtime** patch. It does not modify the shipped `libairplay.so` file.

The Q cluster invokes this primitive at exactly 12 callsites:

```
0x6420
0x6454
0x64a0
0x64d4
0x6508
0x653c
0x6570
0x65ac
0x65e8
0x6624
0x6660
0x669c
```

The old/new values are guarded. This is important: the Q implementation is not performing blind writes.

## Q runtime patch setup

The patch setup routine around `0x6000–0x66xx`:

- validates several expected words;
- resolves `s_serverCopyPropertyCallback` through `dlsym`;
- derives the Q executable layout from that symbol;
- derives other callback/function addresses from that layout;
- constructs replacement pointers/words;
- calls the 4-byte patch primitive 12 times.

This is the concrete mechanism by which the cluster integrates with the DIO host.

## MU0678 translation

The MU0678 cluster should reproduce the mechanism but use:

1. `dlsym` against the running MU0678 DIO process;
2. the MU0678 `s_serverCopyPropertyCallback` symbol as the host anchor;
3. MU0678-native callback addresses/offsets;
4. MU0678-specific expected instruction words;
5. MU0678-specific replacement addresses;
6. the same guarded `mprotect + 32-bit write` primitive.

Do not port Q absolute addresses.

Do not port Q callback-relative offsets.

Do not blindly port the 12 Q patch words.

The Q patch primitive and lifecycle are portable concepts; the target address/value table is firmware-specific.

## MU0678 AirPlay translation substrate

The cluster can call the already-established MU0678 AirPlay ABI:

```
AirPlayReceiverSessionSetup          0x22fd8
AirPlayReceiverSessionScreen_Create  0x269cc
AirPlayReceiverSessionScreen_Setup   0x26400
AirPlayReceiverSessionScreen_StartSession 0x25fd0
AirPlay_DeriveAESKeySHA512ForScreen  0x28dd8
ServerSocketOpen                     0x79910
NetSocket_CreateWithNative           0x79538
NvssVideoImpl::decode                0x7faf0
```

The cluster should resolve exported functions dynamically where possible and use fixed internal offsets only where the MU0678 library does not export the required function.

## Type-111 path

The Q cluster's type-111 routine begins around `0x3c60`.

It explicitly scans the stream collection and distinguishes 110 and 111:

```
0x3ccc: cmp r1,#0
0x3cd0: cmpeq r0,#110

...

0x3d18: cmp r1,#0
0x3d1c: cmpeq r0,#111
```

The 111 descriptor is retained and the routine then constructs the secondary SessionScreen.

The later sequence statically reaches:

```
0x3f54  -> SessionScreen_Setup
0x3f7c  -> SessionScreen_UpdateState
0x405c  -> ServerSocketOpen
```

The post-accept worker is now concretely traced further. It is created at `0x4128` with start routine `0x38fc`. In that worker:

```
0x395c -> API slot 0x10328 -> SocketAccept
0x399c -> API slot 0x10348 -> dio::OmxVideoImpl::decode
0x3b60 -> API slot 0x101e0 -> AirPlayReceiverSessionScreen_StartSession
```

`ProcessFrames` resolves to slot `0x10318`, but its resolver literal is the only direct occurrence of that slot's address in the binary. `NetSocket_CreateWithNative` similarly resolves to `0x102bc` with no second direct slot reference. Therefore neither operation is currently proven as a Q worker call; the earlier documentation claiming a direct `ProcessFrames(B, NetSocket)` stage is corrected here.

## MU0678 security translation

Use the MU0678-native recipe already recovered:

```
session + 0x1c0
        |
        +-- 16 bytes
        +
111 negotiated 64-bit value
        |
        v
AirPlay_DeriveAESKeySHA512ForScreen
        |
        +-- key A
        +-- key B
        |
        v
SetSecurityInfo(B,keyA,keyB)
```

The MU0678 `SetTimeSynchronizer` implementation is:

```
memcpy(screen + 0x20, p, 12)
```

The Q extra synchronizer arguments must not be copied into the MU0678 implementation because MU0678 ignores them.

## MU0678 listener translation

The secondary listener must use the MU0678 stock screen/media listener ABI recovered at `0x23380`:

```
arg1 = *(uint8_t *)(session+0x51)
arg2 = 1
arg3 = 6
arg4 = 0xFFFFE872
arg5 = fresh bound-result storage
arg6 = 0xFFFD4000
arg7 = fresh socket-fd storage
```

Do not copy the Q listener constants.

Do not use the RAOP event listener at `0x23C48`.

## Decoder translation

Q dynamically resolves:

```
dio::OmxVideoImpl::initialize
dio::OmxVideoImpl::decode
dio::OmxVideoImpl::configure
dio::OmxVideoImpl::start
dio::OmxVideoImpl::stop
dio::OmxVideoImpl::setVisibility
```

MU0678 has no reason to reproduce those OMX objects.

The MU0678 cluster should instead drive the established NvSS path, ultimately reaching:

```
NvssVideoImpl::decode @ 0x7faf0
```

and then use the separate output sink.

## Output

The Q output helper is:

```
/eso/bin/apps/mhi2q-aa-cluster-display --live-carplay
```

with loopback H.264 transport on port 15557.

For MU0678 the logical target is the dedicated:

```
DISPLAYABLE_KOMBI_MAP_VIEW
ID 33
1440 x 455
```

Do not copy Q's 1440x540 geometry or DMDT constants.

## Implementation order

The static implementation is now sufficiently specified to build in this order:

1. preserve the stock MU0678 `libairplay.so`;
2. create a MU0678 CarPlay cluster shared object;
3. load that cluster into the DIO process using a Q-style DIO integration mechanism;
4. resolve MU0678 host callbacks with `dlsym`;
5. implement the guarded runtime patch primitive;
6. construct the MU0678-specific callback/function interposition table;
7. intercept the 111-capable session/property lifecycle;
8. execute the independent SessionScreen B lifecycle;
9. create the MU0678 secondary listener;
10. accept and wrap the socket;
11. perform post-accept StartSession;
12. reproduce the Q post-accept worker boundary;
13. route secondary H.264 through the MU0678-native NvSS/output path;
14. tear down B independently;
15. validate simultaneous 110 + 111 operation on hardware.

## Static closure

What is now proven:

- Q does not replace `libairplay.so` on disk.
- Q loads a CarPlay cluster into DIO.
- Q cluster dynamically resolves AirPlay and DIO APIs.
- Q cluster dynamically resolves the DIO server-property callback.
- Q cluster performs guarded runtime 32-bit interposition.
- Q has exactly 12 calls to its patch primitive.
- Q explicitly detects stream type 111.
- Q creates/sets up a second SessionScreen.
- Q uses a second listener and a dedicated post-accept worker.
- Q directly calls post-accept StartSession and contains a direct OMX decode call.
- MU0678 exposes the required DIO callback symbols.
- MU0678 exposes the required native AirPlay/Screen substrate.
- MU0678 has a native NvSS decode path.

What remains statically unresolved:

- exact MU0678 output/display activation;
- exact indirect Q data/transport path after the direct StartSession call;
- whether Q's resolved ProcessFrames/NetSocket symbols are used through an indirect table rather than direct callsites;
- successful simultaneous 110+111 negotiation;
- actual secondary H.264 decode/display;
- reconnect/teardown stability.

