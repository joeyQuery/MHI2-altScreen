# Trace 017 — MU0678 ScreenSetup ABI and Q callback boundary

## 1. MU0678 ScreenSetup third argument is not a caller input

The exact stock MU0678 export is:

```
AirPlayReceiverSessionScreen_Setup @ 0x26400
size = 96 bytes
```

The first instructions are:

```
0x26400  ldr  r3,[pc,#0x50]
0x26404  push {r4,lr}
0x26408  mov  r4,r0
0x2640c  mov  r0,r1
0x26410  ldr  r1,[pc,#0x44]
...
0x26420  add  r2,sp,#4
...
0x26428  bl   0x15b5c
```

The critical fact is `0x26420`:

```
r2 = sp + 4
```

The only internal setup helper call is then made with that newly constructed `r2`.

Therefore the caller-supplied `r2` is overwritten before it can be consumed.

### Consequence

For MU0678, the exported function's effective input contract is not:

```
Setup(screen, setupObject, CreateResult)
```

where the third caller argument affects setup.

Instead, the function internally creates/uses its own stack result/output storage.

Therefore:

- Q's `Setup(B, stream111, CreateResult)` remains part of the Q functional specification;
- MHI2 must **not** invent a Q-style third-argument dependency;
- passing zero as the third register to the MU0678 export does not change this function's internal setup path;
- the previous "MHI2 CreateResult discrepancy" is closed at the ABI/data-flow level.

This is a stronger result than merely observing that the current adapter happens to pass zero.

## 2. Q SessionScreen callback boundary is externally dispatched

The Q DIO loader exports:

```
CDIOManager::s_sessionScreenCreatedCallback(void*) @ 0x154CDC
```

Its body eventually calls:

```
AirPlayReceiverSessionScreenSetDelegate
```

The callback has a dynamic global relocation:

```
0x1A68B0
R_ARM_GLOB_DAT
-> 0x154CDC s_sessionScreenCreatedCallback
```

A complete executable xref sweep of the Q DIO loader finds no local direct call into this callback.

The other relevant DIO operations are independently located:

```
ScreenCreate
ScreenRegisterDelegates
ScreenRegister
```

but those operations are reached from the DIO server-property path rather than from the type-111 cluster setup function.

The loader's own delegate-registration calls are also separate:

- `AirPlayReceiverSessionSetDelegate` is called from `s_sessionCreatedCallback`;
- `AirPlayReceiverServerSetDelegate` has its own server-property callsite;
- neither local registration path directly invokes `s_sessionScreenCreatedCallback`.

### Static conclusion

The missing edge is genuinely **cross-module/external**:

```
SessionScreen exists
      |
      v
external/native dispatch
      |
      v
s_sessionScreenCreatedCallback
      |
      v
AirPlayReceiverSessionScreenSetDelegate
```

The Q DIO binary does not contain the missing dispatcher.

This is therefore not an untraced local DIO function that can be copied into MHI2. The next place to resolve it would be the surrounding Q host/integration binary or the runtime component responsible for dispatching the exported callback.

## 3. Functional replica consequence

The Q functional sequence remains authoritative:

```
detect 111
 -> Create B
 -> UpdateState(B,1)
 -> Setup(B,111,CreateResult)
 -> time sync
 -> security
 -> secondary listener
 -> 111/dataPort response
 -> accept
 -> NetSocket
 -> ProcessFrames(B,...)
```

But the MHI2 translation is now precise:

- use MU0678's native `ScreenSetup` ABI;
- do not manufacture a third-argument CreateResult dependency;
- keep Q's callback relationship separate from the unresolved cross-module dispatcher;
- do not add `StartSession(B)`;
- do not add the Q `+0x38` diagnostic state.

## Status

### Closed offline

- MHI2 ServerSocketOpen ABI/value recipe;
- Q teardown resource ordering;
- MU0678 ScreenSetup third-argument data-flow.

### Precisely bounded, but not locally resolvable from the supplied Q DIO/cluster binaries

- external dispatcher that invokes `s_sessionScreenCreatedCallback`.

### Live-only

- genuine `changeModes=111`;
- secondary socket acceptance on MHI2;
- complete 111 ProcessFrames -> NvSS -> VC/KOMBI display.
