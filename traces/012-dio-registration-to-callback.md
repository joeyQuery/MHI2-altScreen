# Trace 012 — DIO Registration / Callback Boundary

## Evidence

In the Q DIO loader, CDIOManager::s_serverCopyPropertyCallback at 0x158680 performs the DIO screen setup.

The relevant sequence is:

    0x158BD4  ScreenCreate(server)
    0x158E20  ScreenRegisterDelegates(...)
    0x158E28  ScreenRegister(server)

Immediately before ScreenRegisterDelegates, the function constructs the delegate/callback argument frame, including stack arguments at sp+0x1C, sp+0x20 and sp+0x2C.

Separately, s_sessionScreenCreatedCallback is exported at 0x154CDC and its body calls the AirPlay operation corresponding to AirPlayReceiverSessionScreenSetDelegate.

The callback has a R_ARM_GLOB_DAT relocation at 0x1A68B0.

A code-xref sweep does not find a direct in-loader call to s_sessionScreenCreatedCallback.

## New boundary finding

The DIO ScreenCreate/Register sequence is not a direct call from the type-111 setup block.

It is reached from the DIO server-property callback:

    s_serverCopyPropertyCallback
            |
            +-- ScreenCreate
            |
            +-- ScreenRegisterDelegates
            |
            +-- ScreenRegister
            |
            +-- [external/native screen dispatch]
                     |
                     +-- s_sessionScreenCreatedCallback
                             |
                             +-- AirPlayReceiverSessionScreenSetDelegate

The exact external/native dispatch implementation is still outside the loader's proven code references. In particular, ScreenRegister must not yet be declared to be the direct callback trigger.
