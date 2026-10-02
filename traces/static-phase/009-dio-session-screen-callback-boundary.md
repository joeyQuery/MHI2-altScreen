# Trace 009 — DIO SessionScreen Callback Boundary

## Target

Q DIO loader and its SessionScreen-created callback.

## Evidence

The DIO loader contains:

- `dio::CDIOManager::s_sessionScreenCreatedCallback(void*)` at `0x154CDC`;
- executable ScreenCreate callsite around `0x158BD4`;
- executable ScreenRegisterDelegates callsite around `0x158E20`;
- executable ScreenRegister callsite around `0x158E28`.

The callback body receives a pointer, validates the current DIO/screen state, constructs the callback argument state, and invokes the resolved AirPlay operation corresponding to `AirPlayReceiverSessionScreenSetDelegate`.

The callback is present in the loader's dynamic symbol table and has a relocation at `0x1A68B0` (`R_ARM_GLOB_DAT`).

A code-xref sweep of the DIO loader did not find a direct in-loader call into `s_sessionScreenCreatedCallback`.

## Conclusion

The proven architecture is:

`SessionScreen creation` → **external/cross-module callback dispatch** → `s_sessionScreenCreatedCallback` → `AirPlayReceiverSessionScreenSetDelegate`

The DIO loader's ScreenCreate/Register operations are independently proven, but their exact causal linkage to the type-111 SessionScreen creation is not proven from this loader alone.

Do not claim that ScreenRegister itself is the direct type-111 callback trigger.
