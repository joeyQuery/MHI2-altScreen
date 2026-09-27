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

### 3. Q CarPlay cluster directly contains a process-launch edge
libmhi2qcarplaycluster.so imports spawnl.

A direct ARM callsite exists at 0x4578 to the spawnl PLT entry. Immediately before the call:
- r0 = 1
- r1 = r4
- r2 = r4
- r3 = pointer to the embedded string "--live-carplay"
- stack arg at [sp] is zero.

This is a concrete spawnl(P_WAIT, path, argv0, "--live-carplay", NULL)-shaped invocation. The exact semantic identity of r4 is determined by the surrounding launch helper/data flow; the call itself is not speculative.

The same Q cluster contains the literal executable path:

/eso/bin/apps/mhi2q-aa-cluster-display

and the literal mode flag:

--live-carplay.

Thus the previously unresolved question “does the Q CarPlay cluster actually have a process-launch edge for the packaged display helper?” is now closed at the call-site level: the cluster has a real spawnl launch path with the live-CarPlay flag. What remains is only the exact data-flow proof that the r4 path at this call is that embedded helper path on the specific branch.

### 4. The helper executable explicitly recognizes live-CarPlay mode
mhi2q-aa-cluster-display has a direct strcmp in main at 0x103140 comparing argv[1] against the embedded string --live-carplay.

When that comparison matches, the helper selects a geometry state containing:
- width = 1440
- height = 540

The helper then initializes its display stack through dynamically resolved:
- display_init
- display_create_window
- screen_set_window_property_iv
- screen_get_window_property_iv
- screen_create_window_buffers
- screen_post_window
- screen_create_window
- screen_create_window_type
- EGL entry points.

This is hard static evidence that --live-carplay is not a dead label: it selects a dedicated display mode in the packaged helper.

### 5. The helper has a direct screen-output implementation
The helper contains:
- screen_create_window
- screen_create_window_type
- screen_create_window_buffers
- screen_post_window
- buffer/property queries
- screen_destroy_window_buffers
- screen_destroy_window.

It also contains the literal usage: %s stream.h264, showing that the executable has an H.264 stream-input mode in addition to its live-CarPlay mode.

## Consequence for MHI2 replication
The Q display boundary is now materially more closed than the previous trace established.

Proven static architecture:

111 SessionScreen -> secondary listener -> accepted socket -> ProcessFrames -> Q video/output integration -> spawnl(..., "--live-carplay") -> mhi2q-aa-cluster-display -> QNX screen window

The remaining uncertainty is no longer whether a launch mechanism exists. It is:
1. exact r4 path data-flow at the 0x4578 call;
2. exact handoff between the Q cluster's frame-processing state and the helper process;
3. exact mapping from the helper's QNX screen window to the physical cluster display.

The earlier statement that the Q display boundary was merely a “candidate helper” is therefore obsolete. The helper launch edge itself is proven.

## MHI2 consequence
Do not assume a second native ScreenCreate is required for the 111 display sink.

The Q reference now provides a stronger functional translation target: a secondary CarPlay stream can terminate in a dedicated H.264/display process using the QNX Screen API. On MHI2, the equivalent can be implemented either by:
- a native ScreenStream/NvSS path if it can be bound to the intended output, or
- a dedicated helper process analogous to the Q binary.

The exact physical VC mapping remains a separate hardware/runtime question and must not be inferred from DMDT ID 33 or any generic NVSS output-device constant.
