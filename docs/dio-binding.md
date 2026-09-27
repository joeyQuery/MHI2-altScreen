# DIO binding

Q's DIO loader resolves `ScreenCreate`, `ScreenRegisterDelegates`, and `ScreenRegister`.

It contains `dio::CDIOManager::s_sessionScreenCreatedCallback` at approximately `0x154CDC`. The callback calls `AirPlayReceiverSessionScreenSetDelegate(new_session_screen, delegate)`.

The native registration path is `ScreenCreate -> ScreenRegisterDelegates -> ScreenRegister`.

These are separate object layers: AirPlay SessionScreen and native DIO Screen are not treated as identical objects.

The current MHI2 implementation uses different delegate-copy/repoint bookkeeping. That is not yet proven equivalent to Q.
