# Trace 006 — unresolved-area attack: ScreenStream, DIO, and secondary setup

## 1. ScreenStreamCreate +0x38: semantic priority changed

The Q wrapper at `0x4710` first calls the original `ScreenStreamCreate`, then copies four bytes from the object reached through its second argument:

```c
memcpy(&private_q_global, *(arg1) + 0x38, 4);
```

The copy is performed through the Q cluster's `memcpy` PLT entry. The destination is a private Q-cluster global. A full xref sweep of the Q image found no subsequent read of that global. Therefore:

- the `+0x38` field is proven to be observed;
- its semantic name remains unknown;
- it is **not proven to be a functional dependency** of the Q secondary-display path;
- it should not be treated as a required MHI2 replica step at this stage.

This supersedes the weaker wording that implied the capture itself was functionally required.

## 2. Q secondary setup callsite is more concrete than the high-level lifecycle

The Q secondary path contains a contiguous sequence with these operations:

1. internal Create wrapper returns the secondary SessionScreen object;
2. `UpdateState(B, 1)` is called;
3. `ScreenSetup(B, stream111, CreateResult)` is called;
4. a four-argument internal wrapper is invoked for the time-synchronizer stage;
5. AES/SHA512 key material is derived using the session security material and negotiated 64-bit value;
6. an internal security wrapper consumes the two derived 16-byte buffers;
7. `ServerSocketOpen` is called with the Q seven-argument ABI;
8. the returned listener is used to construct the type-111 response entry.

The important new point is that Q's direct callsites contain internal wrapper layers around some of the exported AirPlay operations. Therefore the replica should match **observable argument flow and ordering**, not assume every step is a direct `dlsym` function-pointer call.

The exact Q `StartSession(B)` call has not yet been isolated in this contiguous setup block. Do not invent its location.

## 3. Q DIO loader: native Screen creation and registration are now callsite-proven

In `dio_manager.mhi2q_carplay_loader`, the same native helper contains actual calls to:

- `ScreenCreate` at approximately `0x158BD4`;
- `ScreenRegisterDelegates` at approximately `0x158E20`;
- `ScreenRegister` at approximately `0x158E28`.

These are not merely exported-symbol names: the ARM callsites were found and their call targets resolved through the loader's relocation/symbol table.

This upgrades the evidence from “API surface exists” to “the loader contains executable creation/delegate-registration/registration callsites”.

What remains unresolved is the exact caller-to-secondary-SessionScreen chain: a complete call-by-call proof that these operations are invoked specifically for the Q CarPlay type-111 SessionScreen, rather than for a broader DIO screen-management path, is still required.

## 4. Socket ABI: Q versus current MHI2 implementation

Q's secondary listener call is proven as:

```c
ServerSocketOpen(
    *(uint8_t *)(session+0x55),
    1,
    6,
    0x17D4,
    sp+0x30,
    r5,
    sp+0x34
);
```

The current MHI2 duplicate listener callsite uses the same resolved function pointer but a different register/stack argument layout. That difference is real binary evidence, but it cannot yet be classified as an MHI2 bug because the stock MU0678 `libairplay.so` containing its native `ServerSocketOpen` callsite is not currently available.

Therefore the correct status is:

- Q ABI: **proven**;
- current MHI2 duplicate ABI: **proven**;
- equality between them: **unproven**;
- next evidence required: stock MU0678 `ServerSocketOpen` callsite or a runtime ABI probe.

## 5. Teardown

The current MHI2 implementation's teardown wrapper calls the stock AirPlay teardown and then conditionally tears down its alternate state. This is an implementation fact, not proof of Q ordering.

The Q binary contains separate secondary Stop/Delete function pointers and cleanup paths, but the complete normal-session teardown ordering across AirPlay, secondary SessionScreen, socket, ScreenStream, and DIO/decoder remains unresolved.

## Result

The unresolved set is now narrower:

1. exact Q `StartSession(B)` location/arguments;
2. exact Q secondary SessionScreen -> DIO native Screen creation/registration chain;
3. stock MHI2 `ServerSocketOpen` ABI;
4. Q normal teardown ordering;
5. successful live 111 run through secondary decode/display.

`ScreenStreamCreate +0x38` semantic meaning is still unknown, but its functional priority has been downgraded because the captured value is not read elsewhere in the Q cluster.