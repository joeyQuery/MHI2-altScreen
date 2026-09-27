# DIO binding

## Q callback boundary

Q exports:

```
dio::CDIOManager::s_sessionScreenCreatedCallback(void*) @ 0x154CDC
```

The callback invokes:

```
AirPlayReceiverSessionScreenSetDelegate
```

The callback has a dynamic `R_ARM_GLOB_DAT` relocation at `0x1A68B0`.

A complete executable xref sweep of the Q DIO loader finds no local direct call into the callback. The loader separately contains executable callsites for:

- `ScreenCreate`
- `ScreenRegisterDelegates`
- `ScreenRegister`

Those operations are reached from the DIO server-property path, not directly from the type-111 cluster setup function.

The loader also has separate session/server delegate registration paths. Neither is a local direct caller of `s_sessionScreenCreatedCallback`.

### Proven boundary

Therefore the callback edge is:

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

The exact dispatcher is not contained in the supplied Q DIO loader. It must be sought in the surrounding host/integration component if further static evidence becomes available.

## Native Screen layer

Q's `ScreenCreate -> ScreenRegisterDelegates -> ScreenRegister` surface is real, but symbol/callsite presence alone does not prove that this sequence is the direct consequence of type-111 SessionScreen creation.

MHI2's native Screen registry is independently multi-object capable. The remaining MHI2 implementation task is therefore to associate the secondary AirPlay stream with the correct native Screen without changing the primary 110 selection.

Do not equate the AirPlay SessionScreen object with the DIO/native Screen object.
