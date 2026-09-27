# Trace 014 — MHI2 ServerSocketOpen ABI Correction

## Concrete correction

The previous conclusion that stock MHI2 `ServerSocketOpen` has a special 16-argument ABI was wrong.

The function prologue is:

    push {r4-r11,lr}
    sub  sp, #0x44

The saved-register push consumes 0x24 bytes, so the total caller-SP displacement is:

    0x24 + 0x44 = 0x68

Therefore the callee's:

    [sp+0x68]
    [sp+0x6c]
    [sp+0x70]

are the ordinary caller stack arguments **5, 6 and 7**.

The MHI2 ABI is therefore the same seven-argument AAPCS shape used by the Q implementation:

    ServerSocketOpen(arg1, arg2, arg3, arg4, arg5, arg6, arg7)

## Proven argument roles

The first three arguments are passed directly into the socket-opening path and match the normal socket tuple:

    arg1 = address-family/domain selector
    arg2 = socket type
    arg3 = protocol

The stock callers establish the expected stream/datagram combinations:

    arg2=1, arg3=6   -> TCP/SOCK_STREAM
    arg2=2, arg3=17  -> UDP/SOCK_DGRAM

The fourth argument is saved at `ServerSocketOpen+0x10`. The function tests it for zero and, when non-zero, constructs the local endpoint used by the bind path. It is therefore the requested local endpoint/port parameter.

The fifth argument is an output pointer. In the successful bind path:

    ldr r3,[sp,#0x68]
    ...
    str r0,[r3]

The sixth argument is consumed by the socket-option path immediately after socket creation. That path uses option value `0x1002`, the standard `SO_RCVBUF` value. Thus arg6 is the receive-buffer/socket-option value passed by the caller. The exact special handling of negative values is implementation-specific, but the call ABI role is proven.

The seventh argument is the socket-output pointer. In the successful path:

    ldr r3,[sp,#0x70]
    str r5,[r3]

where r5 is the created socket descriptor.

So the proven high-level shape is:

    ServerSocketOpen(
        family,
        socket_type,
        protocol,
        requested_endpoint,
        bound_port_out,
        receive_buffer_value,
        socket_fd_out
    )

## Q comparison

The Q secondary listener call is:

    arg1 = *(uint8_t *)(session+0x55)
    arg2 = 1
    arg3 = 6
    arg4 = 0x17d4
    arg5 = sp+0x30
    arg6 = -1
    arg7 = sp+0x34

The current MHI2 adapter call at 0x3608 is:

    arg1 = r4
    arg2 = 1
    arg3 = 6
    arg4 = r4
    arg5 = sp+0x3c
    arg6 = -1
    arg7 = sp+0x40

and the immediately preceding instructions prove:

    mov r3,r4
    ...
    mov r0,r3
    ...
    r4 is the successful return value of the preceding AirPlay operation

The success path explicitly requires that return value to be zero. Consequently the current adapter is passing:

    arg1 = 0
    arg4 = 0

This is materially different from the Q implementation, which passes a real family/domain value and a non-zero requested endpoint.

## What this changes

The previously documented ABI mismatch is disproven.

The current MHI2 adapter does not need a 16-argument calling convention. Its stack layout already matches the seven-argument MHI2 ABI.

The concrete socket defect now visible in the adapter is instead the **values** supplied to the seven-argument call:

    current MHI2:
        family/domain = 0
        endpoint      = 0

versus Q:

        family/domain = session+0x55 byte
        endpoint      = 0x17d4

The remaining platform-specific question is no longer ABI reconstruction. It is identifying which of the two stock MHI2 TCP `ServerSocketOpen` callsites (0x23380 or 0x23c48) is the screen-data listener and copying its proven family/endpoint/buffer/output semantics into the type-111 listener.

## Evidence anchors

Stock MHI2 `ServerSocketOpen`:

    0x79910  function entry
    0x79948  load arg6 from [sp+0x6c]
    0x79994  load saved arg4
    0x79a44  load arg5 from [sp+0x68]
    0x79a80  load arg7 from [sp+0x70]
    0x79a7c  write resulting bound value through arg5
    0x79a84  write created fd through arg7

Stock MHI2 TCP callers:

    0x23380
    0x23c48

Current MHI2 adapter:

    0x3608..0x361c

Q secondary listener:

    0x40a0..0x40d4
