# Secondary listener / ServerSocketOpen

## Q ABI

Q callsite `0x40A0-0x40D8`:

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

This is a proven seven-argument Q callsite. `0x17D4` is only identified as argument 4.

## Stock MU0678 ABI

The stock MU0678 `libairplay.so` is present in the repository dump. Its `ServerSocketOpen` implementation is at `0x79910`, size 788 bytes.

Its prologue saves:

- argument 1 from `r0`;
- argument 2 from `r1`;
- argument 4 from `r3`.

Incoming `r2` is not consumed as an input before being reused internally.

After its `0x44`-byte local frame is established, it reads `[sp+0x68]`, `[sp+0x6C]`, and `[sp+0x70]`. These correspond to caller stack offsets `+0x24`, `+0x28`, and `+0x2C`, i.e. arguments 14, 15, and 16 under the normal ARM argument convention.

Therefore stock MHI2 `ServerSocketOpen` is **not** the Q seven-argument ABI. The current MHI2 replica must use an MHI2-specific adapter/frame rather than copying Q's seven arguments.

The exact semantic names of MHI2 arguments 1/2/4 and 14/15/16 remain to be recovered from the wider socket helper chain.

## Replica consequence

Treat `ServerSocketOpen` as an unavoidable platform-specific translation point:

- Q: seven-argument ABI.
- MHI2: wide ABI with high stack arguments and unused incoming `r2`.

The listener port still feeds the type-111 `dataPort` response descriptor.
