# Trace 008 — Stock MHI2 ServerSocketOpen ABI

## Correction

The earlier interpretation of the stack offsets was wrong.

`ServerSocketOpen` at `0x79910` saves 0x24 bytes of registers and then subtracts `0x44` from SP. The total caller-SP displacement is therefore `0x68`. Consequently callee `[sp+0x68]`, `[sp+0x6c]`, and `[sp+0x70]` are ordinary caller arguments 5, 6, and 7.

The stock MHI2 ABI is therefore a normal **seven-argument AAPCS call**, not a 16-argument ABI.

## Proven argument roles

The proven high-level shape is:

    ServerSocketOpen(
        family,
        socket_type,
        protocol,
        requested_endpoint,
        bound_result_out,
        receive_buffer_value,
        socket_fd_out
    )

Observed stock combinations include `arg2=1,arg3=6` for TCP/SOCK_STREAM and `arg2=2,arg3=17` for UDP/SOCK_DGRAM.

The fourth argument is saved by the function and controls the local endpoint/bind path. The fifth argument is written with the bound-result value. The sixth argument feeds the socket-option path, which uses option `0x1002` (`SO_RCVBUF`); its negative/sentinel handling is implementation-specific. The seventh argument is written with the created socket descriptor.

## Consequence for Q parity

The Q seven-argument call is ABI-compatible in shape. No MHI2 argument-count adapter is required.

Q secondary listener:

    arg1 = *(uint8_t *)(session+0x55)
    arg2 = 1
    arg3 = 6
    arg4 = 0x17d4
    arg5 = output pointer
    arg6 = -1
    arg7 = output pointer

Current MHI2 implementation at `0x3608`:

    arg1 = 0
    arg2 = 1
    arg3 = 6
    arg4 = 0
    arg5 = sp+0x3c
    arg6 = -1
    arg7 = sp+0x40

Thus the current call is structurally ABI-correct, but its family/domain and endpoint arguments are zero. The two stock MHI2 TCP callers that must be distinguished for the screen-data listener are `0x23380` and `0x23c48`.
