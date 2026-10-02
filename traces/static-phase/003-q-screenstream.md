# Trace 003 — Q ScreenStreamCreate

Wrapper `0x4710` calls the original ScreenStreamCreate, then follows its second argument and reads a 32-bit value at `(*arg1)+0x38`, storing it in secondary-stream state.
