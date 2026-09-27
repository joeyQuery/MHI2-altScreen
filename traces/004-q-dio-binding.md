# Trace 004 — Q SessionScreen/DIO binding

Q callback: `dio::CDIOManager::s_sessionScreenCreatedCallback` at approximately `0x154CDC`.

It invokes `AirPlayReceiverSessionScreenSetDelegate(new_session_screen,delegate)`.

Native DIO registration resolves `ScreenCreate -> ScreenRegisterDelegates -> ScreenRegister`.
