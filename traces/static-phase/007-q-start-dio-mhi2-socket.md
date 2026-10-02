# Trace 007 — Q StartSession, DIO callback boundary, and stock MHI2 ServerSocketOpen

## 1. Q `StartSession(B)` is resolved but not called in the type-111 setup path

Q initialization resolves and stores `AirPlayReceiverSessionScreen_StartSession`.

However, the actual type-111 secondary setup function was traced through its complete relevant block:

- `0x3F54`: `AirPlayReceiverSessionScreen_Create`
- `0x3F7C`: `AirPlayReceiverSessionScreen_UpdateState(B, 1)`
- `0x3F90`: `AirPlayReceiverSessionScreen_Setup(B, stream111, CreateResult)`
- `0x3FBC`: `AirPlayReceiverSessionScreen_SetTimeSynchronizer(...)`
- `0x405C`: `AirPlayReceiverSessionScreen_SetSecurityInfo(...)`
- `0x40A0`: secondary listener setup

There is **no call through the resolved `StartSession` pointer in this path**. The only xrefs to the resolved StartSession storage in the Q cluster are its initialization/storage sequence; no type-111 lifecycle callsite was found.

Therefore the previous lifecycle statement that placed `StartSession(B)` after security is corrected: **Q's proven type-111 path does not call `StartSession(B)` directly.**

Do not add a synthetic `StartSession(B)` call to the MHI2 replica merely because the symbol is resolved.

## 2. Q DIO callback boundary is proven; the cross-module trigger is the remaining boundary

`dio_manager.mhi2q_carplay_loader` contains the exported callback:

`dio::CDIOManager::s_sessionScreenCreatedCallback(void *)` at `0x154CDC`.

Its executable body takes the callback's SessionScreen-like argument and calls the imported `AirPlayReceiverSessionScreenSetDelegate` at `0x154D2C` through the loader's dynamic import.

The same loader contains executable callsites for:

- `ScreenCreate` at `0x158BD4`;
- `ScreenRegisterDelegates` at `0x158E20`;
- `ScreenRegister` at `0x158E28`.

The callback itself is represented as a dynamic relocation-backed exported function symbol. A code-xref sweep does not show an in-loader direct call from the type-111 AirPlay lifecycle into the callback. This means the remaining boundary is **cross-module callback dispatch**, not missing local implementation code.

Consequently the strongest defensible architecture is:

`AirPlay SessionScreen creation` → `external/native callback dispatch` → `s_sessionScreenCreatedCallback` → `AirPlayReceiverSessionScreenSetDelegate`

and separately the DIO loader has the native ScreenCreate/Register/Delegate/Register operation surface.

A claim that `ScreenCreate -> ScreenRegisterDelegates -> ScreenRegister` is already proven to be the exact type-111 callback consequence would overstate the evidence.

## 3. Stock MU0678 `ServerSocketOpen` is a different ABI from Q

The actual stock MU0678 `libairplay.so` is now recovered from the repository dump. It is 857727 bytes and exposes:

`ServerSocketOpen` at `0x79910`, size `788` bytes.

Its prologue immediately establishes:

- `r0` saved as `r10`;
- `r1` saved as `r8`;
- `r3` saved to `[sp+0x10]`;
- `r2` is not saved/consumed as an incoming argument.

More importantly, after its `0x44`-byte local stack frame is established, the function reads:

- `[sp+0x68]`;
- `[sp+0x6C]`;
- `[sp+0x70]`.

Those addresses correspond to caller-stack positions `+0x24`, `+0x28`, and `+0x2C`, i.e. stack arguments 14, 15, and 16 under the normal ARM argument numbering (arguments 1–4 in registers, argument 5 at caller SP+0, etc.).

Thus stock MHI2 `ServerSocketOpen` is **not** a seven-argument Q ABI. Its observable implementation consumes at least:

- argument 1 (`r0`);
- argument 2 (`r1`);
- argument 4 (`r3`);
- argument 14 (caller stack +0x24);
- argument 15 (caller stack +0x28);
- argument 16 (caller stack +0x2C).

The implementation does not consume incoming argument 3 (`r2`) before overwriting it for its own internal work.

There is one in-image PLT call to the exported `ServerSocketOpen`, from `dio::CAbortCommand::create`; that call does not establish the Q seven-argument convention. The important result for our replica is the callee ABI itself: **the current MHI2 seven-argument call is not literal stock-MHI2 ABI parity.**

## 4. Consequence for the replica

The socket layer is now a confirmed platform-specific translation point:

- Q: seven-argument `ServerSocketOpen` callsite.
- MHI2 stock: wide ABI with required high stack arguments and an unused incoming `r2`.

The MHI2 replica therefore needs an MHI2-specific adapter that constructs the stock MHI2 call frame. It must not simply copy Q's seven argument registers/stack slots.

The exact semantic names of MHI2 arguments 1/2/4 and 14/15/16 remain to be established from the wider socket helper call chain. The ABI shape itself is now established from the stock implementation.

## Final status

1. Q `StartSession(B)`: **resolved — not called in the proven type-111 path**.
2. Q SessionScreen → DIO boundary: **resolved to an external callback boundary; local callback body and native DIO operations are proven, but the external dispatcher that invokes the callback is outside this loader**.
3. Stock MHI2 `ServerSocketOpen` ABI: **resolved at the calling-convention level; it is not Q's seven-argument ABI**.