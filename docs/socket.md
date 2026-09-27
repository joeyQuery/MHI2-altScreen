# Secondary listener / ServerSocketOpen

## Q ABI

Q's type-111 listener callsite uses a seven-argument call:

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

This is a proven seven-argument Q callsite. `0x17D4` is only identified as argument 4; its semantic meaning is not assumed here.

## Stock MU0678 ABI

The stock MU0678 `libairplay.so` is present in the repository dump. Its `ServerSocketOpen` implementation starts at `0x79910` and is 788 bytes.

The entry code preserves:

- argument 1 from `r0`;
- argument 2 from `r1`;
- argument 4 from `r3`.

The incoming `r2` is not consumed as an input before being reused internally.

After the function's `0x44)-byte local stack frame is established, the implementation reads caller-supplied stack locations corresponding to:

- `+0x24) — argument 14;
- `+0x28) — argument 15;
- `+0x2C) — argument 16.

Therefore stock MHI2 `ServerSocketOpen` is **not** Q's seven-argument ABI. The current replica must not copy Q's register/stack layout literally.

## What is solved vs. not solved

**Solved:** the ABI shape and the exact caller-stack positions consumed by stock MHI2 are known.

**Not yet solved:** the semantic names/values represented by MHI2 arguments 1, 2, 4, 14, 15 and 16. Those require tracing the stock helper chain and/or a native callsite that supplies them.

This distinction matters: the ABI mismatch is a concrete platform boundary, but it is not evidence by itself that any particular Q argument should be assigned to any particular MHI2 argument.

## Replica consequence

Treat `ServerSocketOpen` as an unavoidable platform-specific adapter:

- Q: seven-argument callsite.
- MHI2: wide ABI with high stack arguments and unused incoming `r2`.

The listener created by this call is still the source of the type-111 `dataPort` response descriptor in the Q flow.
