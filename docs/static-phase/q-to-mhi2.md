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

The Q type-111 **SETUP handler** does not call `AirPlayReceiverSessionScreen_StartSession` directly. The working Q implementation does call `StartSession` later from a separate secondary-socket worker, after `SocketAccept` and `NetSocket_CreateWithNative`. Therefore the MHI2 replica must not insert `StartSession(B)` into the initial SETUP hook, but it **must** reproduce the post-accept StartSession stage once the correct secondary Screen object is established.

The proven Q lifecycle is split:

```
SETUP handler:
detect 111
 -> Create B
 -> UpdateState(B,1)
 -> Setup(B,111,CreateResult)
 -> time sync
 -> derive AES
 -> SetSecurityInfo
 -> secondary listener
 -> advertise 111/dataPort

secondary worker:
 -> SocketAccept
 -> NetSocket_CreateWithNative
 -> StartSession(B,screen-context)
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
2. `StartSession` is not called by the traced type-111 SETUP block; it is called later by the separate secondary-socket worker. The post-accept call is therefore functional and must be represented in the replica at the worker boundary.

Both remain useful trace points, but neither is a required implementation operation without new evidence.


## 2026-09-27 output-boundary correction

A deeper audit of the supplied working package changes the native-Screen interpretation.

The Q DIO loader has exactly one direct BL call to its imported ScreenCreate PLT entry: 0x158bd4. The same creation function reaches ScreenRegisterDelegates at 0x158e20 and ScreenRegister at 0x158e28. The known s_sessionScreenCreatedCallback at 0x154cdc only calls AirPlayReceiverSessionScreenSetDelegate; it is not a second Screen factory.

The package also contains a separate production executable, mhi2q-aa-cluster-display, with --live-carplay and dynamic resolution of display_init, display_create_window, screen_create_window, screen_set_window_property_iv, screen_create_window_buffers, screen_post_window, and related QNX screen APIs. The CarPlay cluster binary embeds /eso/bin/apps/mhi2q-aa-cluster-display and --live-carplay, alongside the 111 metadata strings.

Therefore the Q display sink cannot yet be reduced to “create a second native Screen in DIO and feed ScreenStream B into it”. The helper process is now a first-class candidate for the final output boundary.

This does not change the proven Q transport lifecycle:

SETUP 111 -> Create B -> UpdateState -> Setup/security -> secondary listener -> advertise dataPort -> accept -> NetSocket -> post-accept StartSession -> ProcessFrames.

It changes only the display integration conclusion: the MHI2 implementation must reproduce the functional output boundary, not an assumed Q object layout. Do not globally modify ScreenCopyMain, and do not add a speculative second ScreenCreate merely to match an earlier hypothesis.
