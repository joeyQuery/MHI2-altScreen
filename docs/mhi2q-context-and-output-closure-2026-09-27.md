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
