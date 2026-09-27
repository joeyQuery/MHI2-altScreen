# Static closure — display routing and MHI2 output translation — 2026-09-27

## Scope
This trace closes the three remaining static display questions using the supplied Q package and the recovered MU0678 DMDT/Screen facts. It deliberately separates logical DMDT routing from physical hardware claims.

## 1. Q accepted 111 descriptor is the descriptor actually written to

The Q transport state is a single global structure whose base is the same address used by both accept and the writer.

### Listener construction
At 0xad4c:
- the caller supplies port 0x3cc5 = 15557;
- the listener state stores the listening descriptor at transport+0x4;
- the function binds/listens and initializes the transport state.

### Accept
At 0xa100:
- transport+0 is tested for a currently accepted descriptor;
- transport+0x4 is loaded as the listening descriptor;
- accept() is called at PLT 0xfa4 with the listening descriptor and null address/length arguments;
- the returned descriptor is stored at transport+0.

### Writer
At 0xa738 -> 0xa844:
- transport+0 is loaded as the descriptor;
- the queued buffer and byte count are passed to 0xa1e8;
- 0xa1e8 performs the partial-write loop through write() PLT 0xf44.

Therefore the descriptor identity is fully closed:

transport+0x4 = 15557 listening socket

transport+0 = accepted display-helper TCP connection

write(fd=transport+0, ...) = the 111 H.264 loopback output

The helper's connect(127.0.0.1:15557) therefore lands on exactly the descriptor consumed by the Q writer.

## 2. Q helper's display route is explicitly selected

The helper contains the following exact environment setup before loading the display stack:

IVRDISPLAY=SCREEN

It also sets the production library paths and loads:
- libdisplayinit.so
- libscreen.so.1
- libEGL.so.1

It dynamically resolves:
- display_init
- display_create_window
- screen_create_window
- screen_create_window_type
- screen_set_window_property_iv
- screen_get_window_property_iv
- screen_create_window_buffers
- screen_post_window
- buffer/window property and destruction functions.

The helper's live-CarPlay branch selects 1440x540. That geometry is Q-specific and is not transplanted to MU0678.

## 3. Q explicitly activates DMDT context 74

The helper contains an embedded argument vector beginning at 0x7240:

/eso/bin/apps/dmdt
dmdt
sc
74
sb
dc
70
98
101
102
3
20

followed by a 16-byte binary value.

The production helper invokes this DMDT route from its VC-routing functions (p5092_vc_route_restore / p5092_vc_route_poll). Thus context 74 is not inferred from the executable name; the helper itself requests it.

Independent DMDT documentation shows that DMDT contexts can select the logical display composition and that a dedicated VC display context can contain DISPLAYABLE_KOMBI_MAP_VIEW (ID 33). This establishes the nature of the route, while the exact context-ID-to-physical-display relationship remains hardware/runtime-specific.

For MU0678, the recovered live DMDT inventory already contains DISPLAYABLE_KOMBI_MAP_VIEW ID 33 at 1440x455, and contexts 72/73/74/76/77 use that displayable. Therefore the MHI2 target output boundary is the DMDT/KOMBI route represented by displayable 33, not the MMI DISPLAYABLE_EXTERNAL_SMARTPHONE path.

## 4. MHI2 translation is now explicit

The functional translation should reproduce the Q boundary, not its Q-only implementation details.

### Producer
MU0678 111 SessionScreen B remains the source. Preserve:
- Create B;
- UpdateState(B,1);
- Setup/security;
- secondary listener;
- advertise type 111/dataPort;
- accept;
- NetSocket_CreateWithNative;
- post-accept StartSession;
- ProcessFrames(B,socket).

### Decoder
Use the MU0678-native NvssVideoImpl path. Do not copy Q OMX objects or Q ScreenStream object layouts.

### Display sink
Use a dedicated QNX Screen output helper/process analogous to mhi2q-aa-cluster-display, because this avoids the unresolved/unsafe assumption that MU0678's stock ScreenCopyMain() can select a second physical output.

The MU0678 helper should:
1. receive the secondary H.264 access units from the MHI2 B path through a dedicated local transport;
2. create a QNX Screen window with the target VC geometry 1440x455;
3. bind/route that window through the MHI2 DMDT/KOMBI context using the native DMDT/DIO mechanism already present on the target;
4. create buffers and post decoded frames;
5. leave the stock 110 ScreenStream and main-screen selection untouched.

The 1440x455 value is taken from the recovered MU0678 DISPLAYABLE_KOMBI_MAP_VIEW definition. The Q helper's 1440x540 value is not copied.

## 5. Why this is the safe MHI2 output boundary

The MU0678 stock StartSession path creates a ScreenStream and clones properties from the selected main Screen. Its second argument is configuration/context, not a native Screen selector. Consequently, changing StartSession's context argument cannot by itself redirect B to the VC.

The stock ScreenCopyMain() path must therefore remain unchanged. A separate output helper gives B an independent display sink and cannot accidentally redirect the 110 stream.

The direct native ScreenStream route remains possible only if a future binary/runtime trace proves an MU0678-native way to bind a specific non-main Screen to a ScreenStream without altering the main-screen selection. That is not required for the Q-functional translation established here.

## Closure status

1. Accepted descriptor identity: CLOSED. accept() stores the helper connection at transport+0; the writer reads transport+0.
2. Q display routing: CLOSED statically to the logical DMDT route. The helper explicitly invokes DMDT context 74 and sets IVRDISPLAY=SCREEN before constructing the QNX Screen window. The exact physical output association remains a hardware fact, not a binary inference.
3. MHI2 functional output translation: CLOSED as an implementation architecture. The correct MU0678 translation is a parallel 111 H.264 producer plus a dedicated QNX Screen/DMDT output sink using MU0678's 1440x455 KOMBI displayable, while leaving 110 and ScreenCopyMain untouched.

The only remaining work is implementation/runtime validation on the actual MU0678 unit, not an unresolved static architecture question.
