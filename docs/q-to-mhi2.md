# Q to MHI2 functional mapping

The authoritative Q reference is `docs/mhi2q-reference-package-2026-09-27.md`. The table below maps the **proven Q operations**, while explicitly separating diagnostic observations from required replica behaviour.

| Q operation | Q evidence | MHI2 state | Replica action |
|---|---|---|---|
| detect 111 | SETUP scan | present | preserve |
| Create B | SessionScreen_Create | present | preserve |
| UpdateState | `UpdateState(B,1)` | present | preserve |
| ScreenSetup | `Setup(B,111,CreateResult)` | arg3 may be zero | pass CreateResult |
| time sync | Q local state | present | trace exact 12-byte payload |
| AES | stock Derive helper | present | preserve Q ordering with MU0678-native inputs |
| security | SetSecurityInfo | present | preserve |
| listener | 7-arg ServerSocketOpen | different MHI2 ABI | reconstruct MU0678 ABI; do not copy Q registers literally |
| response | 111 + dataPort | present | preserve |
| accept | SocketAccept | present | preserve |
| socket | NetSocket_CreateWithNative | present | preserve |
| frame pump | ProcessFrames | present | preserve |
| ScreenStreamCreate | Q hook observes `(*arg1)+0x38` | diagnostic only | **do not reproduce solely for parity** |
| stream state | `(*arg1)+0x38` | unresolved | investigate only if later evidence proves functional dependency |
| SessionScreen delegate | callback -> SetDelegate | different path | reproduce relationship only after dispatcher is traced |
| native DIO Screen | Create -> RegisterDelegates -> Register | variants | map exact MU0678 equivalent |
| decoder | OMX lifecycle | NvSS | translate decoder primitive only |
| teardown | stop/delete | partial | reproduce only the Q-proven ordering; unresolved portions remain unresolved |

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

## Diagnostic-only Q observations

Two Q observations are deliberately not promoted into replica requirements:

1. `ScreenStreamCreate` reading `(*arg1)+0x38`; no later read of the captured Q-cluster value was found.
2. Q resolving `StartSession`; no call through that pointer occurs in the traced type-111 setup block.

Both remain useful trace points, but neither is a required implementation operation without new evidence.
