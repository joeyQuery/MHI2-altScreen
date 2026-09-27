# ScreenStreamCreate trace

Q patch `0x653C -> 0x4710` wraps `ScreenStreamCreate`.

Q first invokes the original function, then follows its second argument and reads a 32-bit value at `(*arg1)+0x38`.

The previous shorthand `return_value+0x38` was incorrect.

The field's semantic name is unresolved; do not label it a screen ID, decoder pointer, or stream type without proof.
