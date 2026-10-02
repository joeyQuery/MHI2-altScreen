# ScreenStreamCreate trace

Q patch `0x653C -> 0x4710` wraps `ScreenStreamCreate`.

Q first invokes the original function, then follows its second argument and reads a 32-bit value at `(*arg1)+0x38`.

The previous shorthand `return_value+0x38` was incorrect.

The field's semantic name is unresolved; do not label it a screen ID, decoder pointer, or stream type without proof.


## Functional-priority result

The Q wrapper copies the four-byte value into a private Q-cluster global. A complete xref sweep found no subsequent read of that global in the same Q image. Therefore the `+0x38` field remains semantically unresolved, but the capture itself is not proven to participate in the functional secondary-display path. It should not be reproduced on MHI2 solely for parity.
