# Trace 015 — Stock MHI2 TCP listener identity

## Result

The two TCP `ServerSocketOpen` calls inside stock MU0678 `AirPlayReceiverSessionSetup` are not equivalent.

### 0x23C48 — RAOP event listener

Call construction:

    arg1 = *(uint8_t *)(session + 0x51)
    arg2 = 1
    arg3 = 6
    arg4 = 0xFFFFEC77 = -5001
    arg5 = session + 0xD0B
    arg6 = -1
    arg7 = session + 0xFAF

The decisive value is arg4: `0xFFFFEC77 = -5001`.

This matches the conventional AirPlay/RAOP audio event TCP port 5001. External AirPlay documentation identifies TCP 5001 as the audio-event channel. Therefore this branch is the audio/RAOP event listener, not the CarPlay screen-data listener.

### 0x23380 — remaining TCP listener

Call construction:

    arg1 = *(uint8_t *)(session + 0x51)
    arg2 = 1
    arg3 = 6
    arg4 = 0xFFFFE872
    arg5 = local output pointer
    arg6 = 0xFFFD4000
    arg7 = local output pointer loaded from [sp+0x48]

Thus it is the other TCP listener created by the AirPlay session setup path.

Because the 0x23C48 TCP listener is independently identified as the RAOP event listener, the 0x23380 TCP listener is the screen/media-side TCP listener in the stock MHI2 setup architecture.

## Family argument

Both TCP branches load:

    ldrb r0,[session,#0x51]

so MHI2's family/domain argument is the byte at `session + 0x51`.

Q's secondary listener uses `session + 0x55`.

Therefore the MHI2/Q difference is a Session structure offset, not an ABI difference.

Do not hard-code `2` or `10`; preserve the value from the MHI2 session object exactly as stock MHI2 does.

## Requested endpoint

The stock MHI2 screen/media listener supplies:

    arg4 = 0xFFFFE872

This is the exact value constructed by the stock MHI2 caller. It must be treated as the MHI2 platform value rather than replaced by Q's `0x17D4` merely because the Q listener uses that value.

The stock socket implementation tests arg4 for zero and, when negative, takes its absolute value before constructing the local endpoint. Therefore the screen/media branch's effective requested endpoint is **60302** (`abs(0xFFFFE872)`), while the event branch's `0xFFFFEC77` becomes **5001**. This confirms that the two TCP calls request different local TCP endpoints.

## Receive-buffer argument

The screen/media listener supplies:

    arg6 = 0xFFFD4000

The RAOP event listener supplies:

    arg6 = -1

Therefore MHI2's stock screen/media listener also differs from Q in arg6. Q's secondary listener uses `-1`.

The current MHI2 adapter's `arg6 = -1` is consequently not a literal stock-MHI2 replica of the `0x23380` screen/media listener.

## Replica mapping

The correct MHI2-side equivalent to Q's secondary `ServerSocketOpen` is therefore based on the `0x23380` branch, not `0x23C48`:

    MHI2:
        arg1 = *(uint8_t *)(session + 0x51)
        arg2 = 1
        arg3 = 6
        arg4 = 0xFFFFE872
        arg6 = 0xFFFD4000

with fresh local output storage for arg5 and arg7, following the stock 0x23380 call pattern.

This is a stronger replica target than copying the Q constants:

    Q:
        arg1 = *(uint8_t *)(session + 0x55)
        arg2 = 1
        arg3 = 6
        arg4 = 0x17D4
        arg6 = -1

## Remaining socket question

The identity of the TCP branch is now resolved.

The only remaining question is the semantic representation of the MHI2 `0xFFFFE872` endpoint and `0xFFFD4000` receive-buffer value inside the socket implementation. Their caller values are concrete and recovered; they should not be replaced with Q values without evidence.