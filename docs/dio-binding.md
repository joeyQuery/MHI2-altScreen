# DIO binding

Q's DIO loader resolves `ScreenCreate`, `ScreenRegisterDelegates`, and `ScreenRegister`.

It contains `dio::CDIOManager::s_sessionScreenCreatedCallback` at approximately `0x154CDC`. The callback calls `AirPlayReceiverSessionScreenSetDelegate(new_session_screen, delegate)`.

The native Screen symbols therefore establish the Q-side DIO integration surface. The complete call-by-call proof that this exact path creates/registers the secondary native Screen is not yet recorded here; do not promote symbol resolution into a stronger claim.

These are separate object layers: AirPlay SessionScreen and native DIO Screen are not treated as identical objects.

The current MHI2 implementation uses different delegate-copy/repoint bookkeeping. That is not yet proven equivalent to Q.
