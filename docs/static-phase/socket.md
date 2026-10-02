# Secondary listener / ServerSocketOpen

## Q ABI

Q's type-111 listener callsite uses a seven-argument AAPCS call:

```c
ServerSocketOpen(
    *(uint8_t *)(session+0x55),
    1,
    6,
    0x17D4,
    sp+0x30,
    -1,
    sp+0x34
);
```

The important point is that Q is not using a proprietary argument-count ABI here. It is the same seven-argument ARM calling shape as stock MU0678.

## Stock MU0678 ABI

Stock MU0678 `ServerSocketOpen` starts at `0x79910`.

The prologue is:

```asm
push {r4-r11,lr}
sub  sp, #0x44
```

The saved-register push consumes `0x24` bytes, so the callee's `[sp+0x68]`, `[sp+0x6c]`, and `[sp+0x70]` are the ordinary caller stack arguments 5, 6 and 7.

Therefore the MHI2 ABI is:

```c
ServerSocketOpen(
    family,
    socket_type,
    protocol,
    requested_endpoint,
    bound_result_out,
    receive_buffer_value,
    socket_fd_out
);
```

Proven roles:

- arg1: address-family/domain selector;
- arg2: socket type;
- arg3: protocol;
- arg4: requested local endpoint;
- arg5: bound-result output;
- arg6: receive-buffer/socket-option value;
- arg7: created socket descriptor output.

Stock combinations establish `arg2=1,arg3=6` as TCP/SOCK_STREAM and `arg2=2,arg3=17` as UDP/SOCK_DGRAM.

## The exact MHI2 CarPlay screen/media listener

Static callsite analysis found eight direct calls through the ServerSocketOpen PLT. The two relevant TCP calls inside `AirPlayReceiverSessionSetup` are:

### `0x23C48` — RAOP event listener

```
arg1 = *(uint8_t *)(session + 0x51)
arg2 = 1
arg3 = 6
arg4 = 0xFFFFEC77 = -5001
arg5 = session + 0xD0B
arg6 = -1
arg7 = session + 0xFAF
```

The requested endpoint is 5001, identifying this branch as the AirPlay/RAOP event listener.

### `0x23380` — screen/media TCP listener

```
arg1 = *(uint8_t *)(session + 0x51)
arg2 = 1
arg3 = 6
arg4 = 0xFFFFE872
arg5 = local output storage
arg6 = 0xFFFD4000
arg7 = local output storage
```

This is the other TCP listener in the AirPlay session setup and is therefore the stock screen/media listener. Its endpoint value is MHI2-native and must not be replaced by the Q constant.

The stock socket implementation takes the absolute value of the negative endpoint before constructing the local endpoint, so `0xFFFFE872` corresponds to effective endpoint **60302**.

The MHI2 screen/media listener therefore has a concrete replica target:

```c
ServerSocketOpen(
    *(uint8_t *)(session + 0x51),
    1,
    6,
    0xFFFFE872,
    &bound_result,
    0xFFFD4000,
    &socket_fd
);
```

Fresh local output storage should follow the stock `0x23380` call pattern.

## Q-to-MHI2 translation

Q uses:

```
family/domain = *(uint8_t *)(session + 0x55)
endpoint      = 0x17D4
receive buf   = -1
```

MHI2 stock screen/media uses:

```
family/domain = *(uint8_t *)(session + 0x51)
endpoint      = 0xFFFFE872
receive buf   = 0xFFFD4000
```

These are platform-specific object/layout values. Do not copy Q's constants into MHI2.

The current MHI2 adapter previously used zero for the family and endpoint and `-1` for arg6. That is not a literal replica of the proven stock MHI2 screen/media listener and should be corrected when the runtime patch is applied.

## Static status

**Solved:** ABI shape, relevant stock callsite, family source, requested endpoint, receive-buffer argument, and output-pointer pattern.

**No longer an unresolved socket ABI problem.**

The remaining runtime question is whether the corrected secondary listener receives the iPhone's type-111 connection once `changeModes=111` is actually observed.
