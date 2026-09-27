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

## New closure: live-CarPlay TCP handoff

### Helper side
- mhi2q-aa-cluster-display compares argv[1] with --live-carplay at 0x103140.
- Equality selects port constant 0x3cc5 = 15557; the alternate file-input path uses 0x3cc3 = 15555.
- The live branch creates socket(AF_INET=2, SOCK_STREAM=1, protocol=0) at 0x103508.
- It builds sockaddr for 127.0.0.1:15557 and calls connect() at 0x103558.
- Therefore live-CarPlay input is an active localhost TCP connection, not stdin, a file, or an inherited decoder pipe.

### Cluster side
- libmhi2qcarplaycluster.so passes 0x3cc5 (15557) at 0x4168 into its internal loopback/listener setup function at 0xad4c.
- The cluster contains the H.264 viewer strings: loopback ready, viewer connected, waiting for complete IDR, non-IDR unsynchronized, codec configuration, viewer synchronized, and viewer disconnected.
- The cluster then verifies the display executable and reaches the real spawnl call at 0x4578 with --live-carplay.

### Closed functional architecture
111 SessionScreen -> secondary listener -> accepted socket -> ProcessFrames -> Q H.264/video integration -> localhost TCP producer :15557 -> spawn mhi2q-aa-cluster-display --live-carplay -> connect 127.0.0.1:15557 -> QNX Screen window.

The exact writer-thread function that performs each queued access-unit write onto the 15557 connection remains the final local code trace. The producer/consumer endpoint, mode selection, and display-process boundary are proven.

### MHI2 consequence
Do not add a speculative second native ScreenCreate solely to reproduce Q. The authoritative Q reference now supplies a concrete display-boundary model: secondary H.264 access units can be exposed through a local TCP producer and consumed by a dedicated display helper running an explicit alternate-display mode. On MHI2, direct NvSS/ScreenStream binding remains preferable only if it can be shown to select the intended output without modifying 110. Otherwise a helper can reproduce the Q boundary.

## Final local writer-thread trace (static)

### 6. The Q loopback producer has a dedicated pthread
The cluster's loopback setup reaches a real pthread_create call at 0x4128.

Immediately before the call:
- r0 = r4 + 0x14 (thread-id storage)
- r1 = r6 (pthread attribute/context value)
- r2 = 0x38fc (resolved from the PC-relative literal at 0x463c)
- r3 = r6 (thread argument)

Therefore the created thread's start routine is statically identified as 0x38fc, and its argument is r6.

### 7. Start routine 0x38fc is a queue/condition-driven worker
At 0x38fc the routine:
- locks a global mutex through the pthread_mutex_lock PLT entry;
- obtains three fields from the worker state ([r4], [r4+4], [r4+8]);
- enters a condition/timed-wait path through a resolved callback;
- processes the returned work state and invokes additional resolved callbacks;
- performs the worker-state cleanup/reset path before returning.

The routine itself does not contain a direct call to the imported write symbol. The imported write PLT callsite is elsewhere in the library (notably 0x6b68 and 0xa36c). Consequently, the static evidence proves the dedicated worker and its queue/condition machinery, but does not justify claiming that 0x38fc directly executes write(fd,buf,len).

This distinction matters: the remaining local code boundary is an indirect callback/function-pointer edge between the worker's queued item and the socket-write primitive, not an unresolved question about whether a writer thread exists.

### 8. What is now proven vs. not proven
Proven:
- loopback listener port = 15557;
- helper connects to 127.0.0.1:15557;
- cluster creates a dedicated pthread at 0x4128;
- thread start routine = 0x38fc;
- worker uses mutex/condition/state machinery;
- cluster contains explicit writer-thread creation failure and queue-saturation/discontinuity diagnostics;
- the cluster contains imported write and concrete write callsites.

Not proven statically from this stripped artifact:
- which exact function-pointer callback in the 0x38fc worker performs the final write of each queued access unit;
- whether that callback is the direct 15557 socket writer or an intermediate transport routine.

No stronger statement should be made without recovering the callback target's data-flow/initialization.

### MHI2 implementation consequence
The Q architecture requires a producer-side worker/queue boundary after H.264 access-unit production. For an MHI2 implementation, this can be reproduced directly with a dedicated worker that owns the secondary-output sink. It must remain independent of the stock 110 ScreenStream path.
