# Trace 008 — Stock MHI2 ServerSocketOpen ABI

## Target

Stock MU0678 `libairplay.so`, exported `ServerSocketOpen`.

## Evidence

The implementation begins at `0x79910` and is 788 bytes.

At entry it preserves:

- `r0` — argument 1;
- `r1` — argument 2;
- `r3` — argument 4.

The incoming `r2` is not consumed as an input before being reused internally.

The function establishes a `0x44)-byte local stack frame and then reads:

- `[sp+0x68]` → caller stack `+0x24` → argument 14;
- `[sp+0x6C]` → caller stack `+0x28` → argument 15;
- `[sp+0x70]` → caller stack `+0x2C` → argument 16.

Therefore the stock MHI2 ABI is materially wider than the Q type-111 seven-argument callsite.

## Conclusion

The ABI shape is solved.

The semantic identity of arguments 1, 2, 4, 14, 15 and 16 is not yet solved. No Q argument is assigned to one of these slots without further evidence.

The MHI2 implementation therefore requires a native socket adapter rather than literal reuse of the Q seven-argument call layout.
