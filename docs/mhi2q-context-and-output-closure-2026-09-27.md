# MHI2Q context and output closure — 2026-09-27

## Authoritative reference

This trace is based only on the supplied `install.zip` Q implementation.

The target remains:

```
stock libairplay.so
    +
DIO-side CarPlay cluster
    +
parallel 111 SessionScreen
    +
dedicated output
```

Experimental MU0678 `libairplay_*111*` binaries are not used as functional references.

## 1. ProcessFrames argument #3 is an existing AirPlay host context

The Q 111 worker does:

```
0x3b2c: r6 = [r9 + 0x0c]
0x3b30: require r6 != 0
...
0x3ba8: r2 = [r6 + 0x1c8]
0x3bb0: r1 = accepted NetSocket
0x3ba4: r0 = SessionScreen B
0x3bbc: ProcessFrames(r0,r1,r2)
```

Here `r9` is the AirPlay session.

Therefore Q's third ProcessFrames argument is:

```
session + 0x0c
        |
        +-- host context object
             |
             +-- +0x1c8 -> ProcessFrames argument #3
```

This is **not** a Q scratch field created by the 111 worker.

## 2. MU0678 proves what session+0x0c is initialized from

The authoritative MU0678 `AirPlayReceiverSessionCreate @ 0x255b8` allocates `0x1418` bytes and performs:

```
r6 = [r1]
...
session = r4
...
0x25634: [session + 0x0c] = r6
```

Thus the same structural boundary exists on MU0678:

```
session+0x0c = first word of the SessionCreate parameter object
```

This is a concrete MU0678 translation fact.

What is **not** yet proven is that MU0678's corresponding host context has the same `+0x1c8` semantic as Q. That offset must be independently traced before copying it.

## 3. Q post-accept worker is fully closed

The authoritative Q worker sequence is:

```
pthread_create @ 0x4128
        |
worker @ 0x38fc
        |
SocketAccept @ 0x395c
        |
NetSocket_CreateWithNative @ 0x3a70
        |
StartSession(B) @ 0x3b60
        |
ProcessFrames(B, NetSocket, hostContext+0x1c8) @ 0x3bbc
```

The worker also contains the Q OMX decode integration. MU0678 must translate the decoder boundary to its native NvSS implementation rather than copying Q OMX object layouts.

## 4. Q live-CarPlay output helper

The supplied executable:

```
/eso/bin/apps/mhi2q-aa-cluster-display
```

contains a distinct `--live-carplay` mode.

At helper main:

```
0x103134: compare argv[1] with --live-carplay
0x10315c..0x103190: select 1440x540 live-mode geometry/state
```

The live mode then creates an IPv4 TCP socket:

```
0x103508:
    socket(AF_INET=2, SOCK_STREAM=1, protocol=0)
```

It constructs the destination sockaddr with:

```
127.0.0.1
port = 0x3cc5 = 15557
```

and connects at:

```
0x103558 -> connect()
```

Therefore the Q helper consumes its live-CarPlay H.264 transport from:

```
TCP 127.0.0.1:15557
```

## 5. Q display stack

The helper dynamically uses QNX Screen APIs including:

- `screen_create_window`
- `screen_create_window_type`
- `screen_create_window_buffers`
- `screen_post_window`
- window/buffer property APIs
- destruction APIs

The helper also contains its own VC-routing support and calls the Q DMDT executable:

```
/eso/bin/apps/dmdt
```

with the Q-specific routing arguments.

The Q live mode uses 1440x540. That geometry is **not** portable to MU0678.

## 6. Q cluster-to-helper boundary

The Q cluster contains the live-CarPlay helper path and launches:

```
/eso/bin/apps/mhi2q-aa-cluster-display
```

with `--live-carplay`.

The cluster's 111 path also creates the secondary worker and publishes the live-output transport state. The concrete helper-side endpoint is independently proven as TCP 127.0.0.1:15557.

The remaining Q-side question is the exact frame handoff implementation between `ProcessFrames`/OMX and the cluster's loopback writer. The cluster contains explicit loopback-stream state and diagnostics:

```
stream: viewer connected, waiting for complete IDR access unit
stream: non-IDR received while unsynchronized; requesting focus bounce
stream: cannot prime viewer without codec configuration
stream: viewer synchronized at complete IDR access unit
stream: viewer disconnected after %llu bytes
stream: socket creation failed errno=%d
stream: cannot listen on TCP port %u errno=%d
stream: writer thread creation failed (%d)
stream: loopback ready port=%u mode=%s queue=%u bytes packets=%u max=%u
stream: oversized access unit length=%u max=%u
stream: queue saturated after %u ms; marking discontinuity packets=%u bytes=%u length=%u
```

These strings are in the authoritative cluster and establish that the loopback side is a dedicated H.264 access-unit writer with IDR/configuration gating, not merely a generic raw socket.

## 7. MU0678 translation

The closest faithful architecture is therefore:

```
MU0678 AirPlay session
        |
        +-- session+0x0c host context
        |
        +-- 110 unchanged
        |
        +-- 111
             |
             +-- SessionScreen B
             +-- secondary listener
             +-- dedicated worker
             +-- SocketAccept
             +-- NetSocket_CreateWithNative
             +-- StartSession(B,...)
             +-- ProcessFrames(B, NetSocket, MU0678 host-context)
             +-- independent NvSS decode
             +-- H.264 access-unit loopback writer
             +-- dedicated display helper
             +-- MU0678 DMDT/KOMBI route
```

The MU0678 output helper should use the logical target already recovered:

```
DISPLAYABLE_KOMBI_MAP_VIEW
ID 33
1440x455
```

The physical panel mapping remains runtime-dependent.

## 8. What must NOT be copied

- Q `session+0x1c8` until MU0678 host-context semantics are independently proven.
- Q 1440x540 geometry.
- Q DMDT route arguments.
- Q OMX object layout.
- Q-specific listener constants.
- Q absolute addresses.
- experimental MU0678 libairplay patches.

## Static status

### Closed

- Q worker sequence.
- Q ProcessFrames argument roles.
- MU0678 origin of session+0x0c.
- Q helper live mode.
- Q helper TCP endpoint 127.0.0.1:15557.
- QNX Screen/display API boundary.
- Q H.264 loopback-writer evidence.

### Remaining narrow static target

Trace the MU0678 object supplied as `session+0x0c` far enough to determine whether its `+0x1c8` field has a direct equivalent. If it does not, identify the MU0678-native object/context that should be passed as ProcessFrames argument #3.

The second narrow target is to recover the Q loopback writer's exact producer call/data format so the MU0678 writer can be made byte/packet-semantically equivalent where the platforms permit it.


## 9. Context offset is now solved on MU0678

The stock MU0678 `_ScreenThread` gives the exact native `ProcessFrames` third argument.

At `0x204d0`:

~~~
r3 = [session + 0x0c]
r1 = [sp + 0x1c]
r0 = [session + 0x140c]
r2 = [r3 + 0xc8]
ProcessFrames(r0, r1, r2)
~~~

Therefore the platform-specific mapping is:

| Platform | AirPlay session context | ProcessFrames arg #3 |
|---|---:|---:|
| Q reference | `session + 0x0c` | `[context + 0x1c8]` |
| MU0678 stock | `session + 0x0c` | `[context + 0xc8]` |

This is a direct disassembly result on both sides. The correct MU0678 implementation must therefore use `[session+0x0c]+0xc8`, not Q's `+0x1c8`.

This confirms that the Q worker deliberately reuses an existing AirPlay host-context value; the field is not a new secondary-screen ABI.

## 10. Q loopback wire format is now solved

The Q display helper has a strict 32-byte frame-header parser at `0x1023b8`.

It first performs an exact 32-byte read, then requires:

~~~
bytes 0..3 = ASCII "AAF1"
byte 4      = 2
byte 7      = 32
~~~

The Q cluster constructs the same header in its producer at `0xa600` and writes it through the common timed write routine `0xa1e8`.

The producer and consumer are therefore both present in the supplied reference binary; this is not an inferred framing protocol.

### Producer header layout

For the 32-byte live format, the cluster writes:

~~~
offset 0x00..0x03 : 'A' 'A' 'F' '1'
offset 0x04        : 0x02
offset 0x05        : stream mode (1 or 2)
offset 0x06        : flags (arg2, with bit 0 forced when object+0x1d is non-zero)
offset 0x07        : 0x20 (32-byte header)

0x08..0x0b         : object +0x18, big-endian
0x0c..0x0f         : object +0x04, big-endian
0x10..0x13         : object +0x0c, big-endian
0x14..0x17         : object +0x08, big-endian
0x18..0x1b         : object +0x14, big-endian
0x1c..0x1f         : object +0x10, big-endian
~~~

The helper parser reconstructs these fields into its frame metadata object. The payload immediately follows the 32-byte header; the producer passes the payload pointer separately to the same timed write routine, with payload length from the producer object's `+0x04` field.

The parser consumes the exact payload bytes after the header.

### Important distinction

The cluster also contains a 24-byte-header path in the adjacent producer logic at `0xa4e0`. That path is not the `--live-carplay` protocol because the helper's live parser explicitly requires byte 7 == 32. The live-carplay path is the 32-byte `AAF1` format constructed at `0xa600`.

## 11. Resulting faithful MU0678 translation

The secondary worker should therefore call the MU0678-native equivalent of:

~~~
AirPlayReceiverSessionScreen_ProcessFrames(
    B,
    acceptedNetSocket,
    *((void **)(*((void **)(session + 0x0c)) + 0xc8))
)
~~~

The Q `+0x1c8` must **not** be copied to MU0678.

The output side should reproduce the Q producer/consumer framing where useful:

~~~
H.264 access unit
    ↓
32-byte AAF1 header
    ↓
exact payload bytes
    ↓
TCP 127.0.0.1:15557
    ↓
mhi2q-aa-cluster-display --live-carplay
~~~

MU0678 should retain its own display geometry/routing (`DISPLAYABLE_KOMBI_MAP_VIEW`, ID 33, 1440x455) rather than copying Q's 1440x540 geometry.

## Static closure status

### Solved

- Q host-context origin: `session+0x0c`.
- MU0678 host-context origin: `session+0x0c`.
- Q ProcessFrames context offset: `+0x1c8`.
- MU0678 ProcessFrames context offset: `+0xc8`.
- Q live-carplay TCP endpoint: `127.0.0.1:15557`.
- Q live frame header magic: `AAF1`.
- Q live header size: 32 bytes.
- Q producer and consumer are both statically identified.
- Payload follows the 32-byte header and is written using the Q timed write routine.

### Remaining static implementation work

The architecture is now closed. Remaining work is implementation translation: construct the MU0678-compatible external cluster/loader and reproduce the Q secondary lifecycle and output writer using MU0678-native APIs and ABI. Runtime validation remains necessary for actual iPhone 111 negotiation, decode continuity, VC routing, and teardown.
