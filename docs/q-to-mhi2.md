# Q to MHI2 functional mapping

The authoritative Q reference is `docs/mhi2q-reference-package-2026-09-27.md`. The table below maps the **proven Q operations**, while explicitly separating diagnostic observations from required replica behaviour.

| Q operation | Q evidence | MHI2 state | Replica action |
|---|---|---|---|
| detect 111 | SETUP scan | present | preserve |
| Create B | SessionScreen_Create | present | preserve |
| UpdateState | `UpdateState(B,1)` | present | preserve |
| ScreenSetup | `Setup(B,111,CreateResult)` | MU0678 export overwrites caller r2 with its own `sp+4` output storage | use native MU0678 ABI; no Q CreateResult dependency |
| time sync | Q local state | present | preserve Q ordering; use MU0678-native 12-byte payload |
| AES | stock Derive helper | present | preserve Q ordering with MU0678-native inputs |
| security | SetSecurityInfo | present | preserve |
| listener | 7-arg ServerSocketOpen | 7-arg AAPCS; stock screen listener is `0x23380` | use MU0678-native listener values, not Q constants |
| response | 111 + dataPort | present | preserve |
| accept | SocketAccept | present | preserve |
| socket | NetSocket_CreateWithNative | present | preserve |
| frame pump | ProcessFrames | present | preserve |
| ScreenStreamCreate | Q hook observes `(*arg1)+0x38` | diagnostic only | **do not reproduce solely for parity** |
| stream state | `(*arg1)+0x38` | unresolved | investigate only if later evidence proves functional dependency |
| SessionScreen delegate | callback -> SetDelegate | cross-module dispatcher boundary | reproduce relationship only; no unsupported local factory assumption |
| native DIO Screen | Create -> RegisterDelegates -> Register | variants | map exact MU0678 equivalent |
| decoder | OMX lifecycle | NvSS | translate decoder primitive only |
| teardown | Q post-AirPlay cleanup | MHI2 stock path differs | preserve proven Q ordering where implementable |

## Critical lifecycle correction

Q resolves `AirPlayReceiverSessionScreen_StartSession`, but the proven type-111 setup block does **not** call it. Therefore the MHI2 replica must not insert a synthetic `StartSession(B)` merely to make the lifecycle look symmetric with stock MHI2 110.

The proven Q functional sequence is:

```
detect 111
 -> Create B
 -> UpdateState(B,1)
 -> Setup(B,111,CreateResult)
 -> time sync
 -> derive AES
 -> SetSecurityInfo
 -> secondary listener
 -> advertise 111/dataPort
 -> accept
 -> NetSocket_CreateWithNative
 -> ProcessFrames(B,socket)
```

The existing MHI2 110 path remains untouched.

## MHI2-native listener translation now solved statically

The stock MU0678 `ServerSocketOpen` ABI is a normal seven-argument AAPCS call. The relevant stock screen/media listener is the `0x23380` call inside `AirPlayReceiverSessionSetup`:

```
arg1 = *(uint8_t *)(session + 0x51)
arg2 = 1
arg3 = 6
arg4 = 0xFFFFE872
arg5 = local bound-result storage
arg6 = 0xFFFD4000
arg7 = local socket-fd storage
```

The other TCP call at `0x23C48` requests endpoint 5001 and is the RAOP event listener, so it is not the screen-data equivalent.

This closes the previous socket-ABI uncertainty. The Q values (`session+0x55`, `0x17D4`, `-1`) must not be transplanted into MHI2; the MHI2 adapter should reproduce the native `0x23380` value construction.

## Diagnostic-only Q observations

Two Q observations are deliberately not promoted into replica requirements:

1. `ScreenStreamCreate` reading `(*arg1)+0x38`; no later read of the captured Q-cluster value was found.
2. Q resolving `StartSession`; no call through that pointer occurs in the traced type-111 setup block.

Both remain useful trace points, but neither is a required implementation operation without new evidence.
