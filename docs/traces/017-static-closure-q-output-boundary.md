# Static closure — Q alternate-output boundary — 2026-09-27

## Scope
This trace uses only the supplied /mnt/data/install.zip package as the authoritative working Q reference.

## Findings

### 1. Q DIO loader has one direct native ScreenCreate call
In dio_manager.mhi2q_carplay_loader:
- ScreenCreate is an imported symbol.
- A complete scan of the ARM .text section for direct BL targets to the ScreenCreate PLT entry finds exactly one callsite: 0x158bd4.
- The surrounding function then reaches ScreenRegisterDelegates at 0x158e20 and ScreenRegister at 0x158e28.
Therefore the loader demonstrably contains a native Screen creation/registration path, but this scan does not establish a second native ScreenCreate call dedicated to stream 111.

### 2. The known Q SessionScreen-created callback is only a delegate operation
dio::CDIOManager::s_sessionScreenCreatedCallback(void*) is at 0x154cdc (208 bytes in this artifact).
The callback checks DIO/native-screen state, builds a small local callback payload, calls the resolved AirPlayReceiverSessionScreenSetDelegate, and returns.
There is no ScreenCreate, ScreenRegister, listener creation, or explicit 110/111 branch inside this callback.

### 3. The Q package contains a separate CarPlay-aware display executable
mhi2q-aa-cluster-display is a packaged production binary.
Its strings include --live-carplay, IVRDISPLAY, DISPLAY_CATALOG_PATH, libdisplayinit.so, display_init, display_create_window, screen_set_window_property_iv, screen_get_window_property_iv, screen_create_window_buffers, screen_destroy_window_buffers, screen_destroy_window, screen_get_window_property_pv, screen_post_window, screen_create_window, screen_create_window_type, and eglGetDisplay.
The executable dynamically resolves these display/screen entry points rather than importing libscreen directly. It also contains a usage: %s stream.h264 string and decoder/output-format handling.
This is hard evidence that the working Q package contains a separate H.264-to-QNX-screen output component with an explicit CarPlay mode.

### 4. The Q CarPlay cluster embeds the helper's executable path
libmhi2qcarplaycluster.so contains /eso/bin/apps/mhi2q-aa-cluster-display and --live-carplay.
The same binary also contains the 111 metadata strings: altScreen, streamConnectionID, dataPort, features, displays, maxFPS, widthPhysical, heightPhysical, primaryInputDevice.
This establishes that the CarPlay cluster and the dedicated display executable are part of the same production package architecture. It does not, by itself, prove the exact launch call/argument construction for every runtime path.

## Consequence for MHI2 replication
The previous conclusion — Q necessarily creates a second native libscreen Screen object for B — is too strong.
What is proven is narrower:
- Q has a native Screen creation/registration path in DIO.
- Q has a separate CarPlay-aware display process that directly creates/posts QNX screen windows.
- Q CarPlay cluster contains the helper executable path and --live-carplay.
- Q type-111 SessionScreen path is separately responsible for transport, security, accept, and frame processing.
Therefore the remaining Q output association cannot be reduced to finding a second ScreenCreate call in the DIO loader. The dedicated display process is now a first-class static candidate for the final 111 display boundary.

## What is still not proven
Without runtime tracing of the working Q package or a complete cross-reference through the stripped cluster's process-launch/data-flow path, this trace does not claim:
- exactly where mhi2q-aa-cluster-display is spawned;
- whether the helper is used for every 111 session;
- the exact mechanism by which its output is mapped to the physical cluster;
- whether the Q native Screen object and the helper process are both involved.

## MHI2 consequence
Do not implement a speculative second native Screen solely because the earlier static model expected one.
Before changing MU0678 StartSession/ScreenStream logic, compare the existing MHI2 video/display path against the Q helper architecture:

Q: 111 SessionScreen -> accepted socket -> ProcessFrames -> Q video/output integration -> dedicated CarPlay-aware display helper -> QNX screen window

MHI2: 111 SessionScreen -> accepted socket -> ProcessFrames -> ScreenStream / NvSS -> existing DIO/display route

The MHI2 translation should reproduce the functional boundary, not assume that Q's native ScreenCreate path is the display sink.