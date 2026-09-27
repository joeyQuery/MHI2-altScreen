# Trace 016 — Static closure of the MHI2 secondary listener

## Result

The remaining MHI2 `ServerSocketOpen` uncertainty is closed statically.

### Stock ABI

MU0678 `ServerSocketOpen` at `0x79910` is a normal seven-argument AAPCS function:

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

The earlier 16-argument interpretation was caused by forgetting the `push {r4-r11,lr}` stack displacement.

### Screen/media listener identity

The stock AirPlay setup has two TCP listener callsites:

```
0x23C48 -> endpoint -5001 -> RAOP event listener
0x23380 -> endpoint 0xFFFFE872 -> screen/media listener
```

Therefore the screen/media branch is the correct native MHI2 analogue for the Q secondary listener.

Its concrete construction is:

```
arg1 = *(uint8_t *)(session + 0x51)
arg2 = 1
arg3 = 6
arg4 = 0xFFFFE872
arg5 = fresh bound-result output
arg6 = 0xFFFD4000
arg7 = fresh socket-fd output
```

The socket implementation converts the negative endpoint to its effective local endpoint, 60302.

### Q comparison

Q's secondary listener is:

```
arg1 = *(uint8_t *)(session + 0x55)
arg2 = 1
arg3 = 6
arg4 = 0x17D4
arg5 = output
arg6 = -1
arg7 = output
```

This is not a reason to copy Q constants. The functional requirement is the **operation** (independent secondary listener + returned dataPort), while the listener ABI/value construction is translated to the native MHI2 screen/media recipe.

## Consequence

The MHI2 replica no longer has an unresolved socket ABI/value problem.

The remaining static boundaries are:

1. exact external dispatcher for Q's `s_sessionScreenCreatedCallback`;
2. exact MU0678 equivalent of Q's `ScreenSetup(..., CreateResult)` third argument if the current MHI2 implementation does not already preserve the real create result;
3. final MHI2-specific secondary Screen/DIO association;
4. live proof of `changeModes=111` and complete 111 decode/display.

The Q lifecycle remains authoritative: no synthetic `StartSession(B)`, no required `ScreenStreamCreate +0x38` state, and no replacement of the primary 110 path.
