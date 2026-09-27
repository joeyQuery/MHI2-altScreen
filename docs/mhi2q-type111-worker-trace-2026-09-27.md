# Q type-111 post-accept worker trace — 2026-09-27

## Purpose

This is the corrected binary-level trace of the authoritative MHI2Q `install.zip` after the secondary 111 listener is created.

The important result is now exact: the supplied Q cluster directly proves the full post-accept sequence `SocketAccept -> NetSocket_CreateWithNative -> StartSession -> ProcessFrames` inside a dedicated worker. The remaining unknown is the identity of the Q-specific context object passed as ProcessFrames argument #3.

## Worker creation

The type-111 path reaches a `pthread_create` call at cluster address:

```
0x4128
```

The fourth argument is the worker argument. The start routine is constructed as address:

```
0x38fc
```

Therefore the secondary accept/decode worker is:

```
pthread_create(..., start=0x38fc, ...)

```

## Worker state

The worker begins at `0x38fc`.

It establishes a small global worker-state object and loads:

- a state/object pointer at `[r4]`;
- a second object at `[r4+0x4]`;
- a listener/resource at `[r4+0x8]`.

The exact stripped C++ type is not recovered and must not be invented.

## Secondary accept

At `0x395c` the worker calls the function pointer resolved into API slot:

```
0x10328 = SocketAccept
```

The call setup is:

```
r0 = listener/resource
r1 = 10
r2 = worker-local output storage
r3 = 0
```

The return value controls the success/error path beginning at `0x3960`.

This is direct evidence that the 111 connection is accepted by the dedicated worker rather than by the stock MU0678 `_ScreenThread`.

## Post-accept StartSession

The worker reaches `0x3b50`, where it loads the function pointer in API slot:

```
0x101e0 = AirPlayReceiverSessionScreen_StartSession
```

and calls it at:

```
0x3b60
```

The call uses:

```
r0 = worker state object / SessionScreen-B object

r1 = [object + 0x1d0]
r2 = 1

```

The exact stripped type of the `+0x1d0` object is not recovered.

The critical invariant is proven: **StartSession occurs after the secondary accept worker is active.**

## ProcessFrames edge

The same worker directly calls API slot `0x10318` at `0x3bbc`.

The call setup is:

```
r0 = SessionScreen B
r1 = wrapped NetSocket
r2 = Q-specific context value
```

The function pointer is loaded from the resolved API table immediately before the indirect call.

This establishes the actual Q handoff from the accepted secondary connection into the AirPlay SessionScreen frame-processing path. MU0678 should preserve the semantic sequence and translate the context/object ABI natively.

## Remaining unknowns

### Q ProcessFrames context

The third argument is loaded from `[r6+0x1c8]`. The exact stripped C++ object/type represented by that field is not recovered. It must not be invented or mapped to a MU0678 field without an independent trace.

### Q output handoff

The exact frame transport from the ProcessFrames/decoder side into `mhi2q-aa-cluster-display --live-carplay` remains the final narrow static target.

## Correct Q post-accept model

The evidence-supported model is:

```
111 listener

   |

pthread_create @ 0x4128

   |

worker 0x38fc

   |

SocketAccept @ 0x395c

   |

StartSession(B) @ 0x3b60

   |

Q-specific secondary transport/decode path

   |

OmxVideoImpl::decode @ 0x399c

   |

Q output/display integration

```

The exact handoff between `StartSession` and the OMX decode call is still an opaque inter-module/object boundary.

## Consequence for MU0678

The MU0678 implementation must reproduce the **function**, not the previously assumed API sequence:

1. detect 111;
2. create/configure SessionScreen B;
3. create the secondary listener;
4. accept the secondary connection in a dedicated worker;
5. call MU0678 `StartSession(B,...)` after accept;
6. feed the resulting 111 media into an independent MU0678 decoder instance;
7. route decoded frames to the dedicated VC/KOMBI output sink;
8. keep 110 completely untouched.

MU0678's existing stock path:

```
ProcessFrames -> ScreenStreamProcessData -> _ScreenStreamDecodeAll -> NvssVideoImpl::decode
```

remains a valid native translation candidate, but it is **not justified by the Q worker trace as a literal one-for-one API sequence**.

## Evidence boundary

### Closed

- dedicated Q 111 worker exists;
- worker start address is `0x38fc`;
- worker is created at `0x4128`;
- secondary connection is accepted through `SocketAccept`;
- accepted native socket is wrapped with `NetSocket_CreateWithNative`;
- post-accept `StartSession` is direct;
- `ProcessFrames(B, NetSocket, Q-context)` is direct;
- 110 stock path is not replaced by this worker.

### Still requiring deeper static recovery

- exact C++ identity/semantics of the Q ProcessFrames argument #3 (`[r6+0x1c8]`);
- exact relationship between the worker's state object and SessionScreen B;
- exact frame transport into `mhi2q-aa-cluster-display --live-carplay`.

These are narrower than the previous architecture-level uncertainty and are now the correct remaining static targets.
