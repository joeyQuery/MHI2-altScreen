# Secondary listener / ServerSocketOpen

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

The final three arguments are live state/output parameters. `0x17D4` is only identified as argument 4.

The listener port is placed into the type-111 `dataPort` response descriptor.

The current MHI2 port-open abstraction is not literal Q parity.
