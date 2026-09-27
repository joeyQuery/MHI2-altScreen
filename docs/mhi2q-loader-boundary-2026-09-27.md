# MHI2Q loader boundary and MU0678 implementation target — 2026-09-27

## Authoritative fact

The supplied working `install.zip` does not replace `/eso/lib/libairplay.so`.

Its installer explicitly verifies the existing AirPlay library:

```sh
run_mmx "$TOOL" verify "$AIRPLAY_SHA" /eso/lib/libairplay.so ||
    fail "libairplay incompatible"
```

The package's CarPlay-specific executable is:

```
dio_manager.mhi2q_carplay_loader
```

and its dynamic dependency list includes:

```
libmhi2qcarplaycluster.so
libairplay.so
libscreen.so.1
...
```

The loader imports the existing AirPlay APIs, including:

- `AirPlayReceiverSessionChangeModes`
- `AirPlayReceiverSessionScreenSetDelegate`
- `AirPlayReceiverSessionTearDown`
- `ScreenRegisterDelegates`
- `ScreenCreate`
- `ScreenRegister`

The cluster separately resolves the SessionScreen/ScreenStream/security APIs needed for the 111 path.

## What the working Q patch actually replaces

The package receipt records:

```
files/dio_manager.mhi2q_carplay_loader
files/libmhi2qcarplaycluster.so
files/libmhi2qaacluster.so
files/gal.mhi2q_aa_cluster_loader
files/mhi2q-aa-cluster-display
...
```

The installer lifecycle therefore operates at the DIO/application layer.

It does **not** install a new `libairplay.so`.

This is the architectural reference to reproduce on MU0678.

## Functional boundary

The working Q architecture is:

```
                     stock
              /eso/lib/libairplay.so
                       |
        +--------------+---------------+
        |                              |
        v                              v
    normal 110                    external DIO loader
    AirPlay path                        |
                                        v
                              libmhi2qcarplaycluster.so
                                        |
                              +---------+---------+
                              |                   |
                         111 SessionScreen     DIO/Screen
                              |                   |
                         secondary socket     output helper
                              |
                         ProcessFrames
                              |
                         H.264/decoder
                              |
                    local display transport
                              |
                    mhi2q-aa-cluster-display
```

The external loader therefore extends stock AirPlay rather than replacing its implementation.

## MU0678 translation

MU0678 already exposes the native AirPlay/Screen primitives required by the Q architecture.

The established MU0678 map includes:

- `AirPlayReceiverSessionSetup @ 0x22fd8`
- `AirPlayReceiverSessionScreen_Create @ 0x269cc`
- `AirPlayReceiverSessionScreen_Setup @ 0x26400`
- `AirPlayReceiverSessionScreen_StartSession @ 0x25fd0`
- `AirPlayReceiverSessionScreen_ProcessFrames`
- `AirPlayReceiverSessionScreen_StopSession`
- `AirPlayReceiverSessionScreen_SetSecurityInfo`
- `AirPlay_DeriveAESKeySHA512ForScreen @ 0x28dd8`
- `ServerSocketOpen @ 0x79910`
- `NetSocket_CreateWithNative @ 0x79538`
- stock NvSS decode path ending in `NvssVideoImpl::decode @ 0x7faf0`

The implementation should therefore preserve stock `libairplay.so` and call these native primitives from a MU0678-compatible DIO-side extension.

## MU0678 DIO audit — concrete result

The stock MU0678 DIO executable is present in the dump at:

`/eso/bin/apps/dio_manager`

Static ELF inspection establishes:

- size: 649,413 bytes;
- NEEDED includes `libairplay.so`;
- NEEDED includes `libnvss_video.so`;
- NEEDED includes `libsocket.so.3`;
- the dynamic symbol table imports `AirPlayReceiverSessionChangeModes`;
- it imports `AirPlayReceiverSessionScreenSetDelegate`;
- it imports `AirPlayReceiverSessionSetDelegate`;
- it imports `ScreenCreate`;
- it imports `ScreenRegister`;
- it imports `ScreenRegisterDelegates`;
- it imports `AirPlayReceiverServerCreate` / `SetDelegate` / `Control` / `SetProperty`;
- it contains the existing `CDIOManager::s_sessionScreenCreatedCallback` at `0x1362d0`;
- it contains the existing `CDIOManager::s_sessionCreatedCallback` at `0x138fb4`;
- it contains the existing `CDIOManager::s_sessionModesChangedCallback` at `0x139268`;
- it contains the existing `CDIOManager::s_sessionFinalizedCallback` at `0x139424`;
- it contains the existing `CDIOManager::s_changeModeCompletionCallback` at `0x137fc4`.

This materially changes the implementation path: **MU0678 already has the DIO-side AirPlay/Screen host surface that the Q loader extends.** We do not need to invent a separate host process or intercept `libairplay.so` merely to obtain those APIs.

The Q working loader has the same architectural role: it is an executable DIO application with `libmhi2qcarplaycluster.so` as an additional NEEDED dependency while retaining `libairplay.so` as a dependency.

Therefore the most direct MU0678 replica is a MU0678 DIO-manager-compatible executable/loader that preserves the existing DIO callbacks and links a MU0678 cluster component implementing the 111 path.

## Remaining prerequisite: DIO replacement audit

The remaining static prerequisite is the stock MU0678 DIO executable itself:

```
/eso/bin/apps/dio_manager
```

The following must be mapped before implementation:

1. process startup and entry point;
2. dynamic dependencies;
3. DIO configuration loading;
4. AirPlay server/session callback registration;
5. Screen delegate registration;
6. SessionScreen-created callback ownership;
7. existing DIO event dispatch;
8. any ABI-sensitive global/static state;
9. whether a replacement executable can preserve ordinary DIO behaviour while adding the Q-style cluster.

This is the correct analogue of the working Q loader.

## What the old MU0678 libairplay patch means

The supplied MU0678 `libairplay_patched_v6.so` demonstrates a separate technique: append an executable PT_LOAD and redirect selected AirPlay instructions into it.

That is an injection mechanism, not evidence of the Q implementation architecture.

It may become useful if MU0678 cannot support an external DIO loader cleanly, but that is a fallback implementation choice, not the working reference.

## Implementation rule

Until the DIO audit is complete:

**Do not modify or replace MU0678 `libairplay.so` as a proxy for reproducing the working Q patch.**

The functional reference is the DIO loader + cluster architecture from `install.zip`.


## New concrete finding from the current `install.zip`: runtime interposition, not file replacement

The previous statement "the Q implementation does not touch libairplay" needs a precise distinction.

It **does not replace the `libairplay.so` file**. However, `libmhi2qcarplaycluster.so` contains a runtime patch/interposition engine.

Static disassembly proves:

1. The cluster resolves symbols from the running process with `dlsym`.
2. It resolves the DIO callback:
   `_ZN3dio11CDIOManager28s_serverCopyPropertyCallbackEP28AirPlayReceiverServerPrivateP9CFLStringPKvPiPv`.
3. It derives the containing executable/base address from the resolved callback address.
4. It performs byte-level writes/copies into executable/runtime memory and validates expected instruction bytes.
5. The cluster imports `mprotect`, consistent with changing executable-page permissions during this operation.
6. The cluster's type-111 setup function is statically identifiable at approximately `0x3c60`. Its stream scan explicitly distinguishes:
   - `type == 110`
   - `type == 111`
   and retains the 111 descriptor.
7. That function then resolves/calls the SessionScreen APIs and the secondary listener APIs recovered previously.

This is the crucial corrected model:

```
install.zip
   |
   +-- does NOT overwrite libairplay.so on disk
   |
   +-- installs custom dio_manager + carplaycluster
                    |
                    +-- dlsym() running AirPlay/DIO symbols
                    |
                    +-- runtime interposes selected code/callbacks
                    |
                    +-- executes parallel 111 SessionScreen path
```

So the correct MU0678 implementation target is **not** "leave libairplay untouched in memory." It is "leave the stock file intact, then reproduce the Q runtime interposition strategy against the MU0678 ABI."

### Q cluster resolver map

The current binary contains a bulk `dlsym` resolver beginning around `0x5828`. The following symbols are resolved into internal function-pointer slots:

- `AirPlayReceiverSessionSetup`
- `AirPlayReceiverSessionTearDown`
- `AirPlayCopyServerInfo`
- `AirPlayReceiverSessionSendCommand`
- `AirPlayReceiverSessionForceKeyFrame`
- `ScreenStreamCreate`
- `dio::OmxVideoImpl::initialize`
- `dio::OmxVideoImpl::decode`
- `dio::OmxVideoImpl::configure`
- `dio::OmxVideoImpl::start`
- `dio::OmxVideoImpl::stop`
- `dio::OmxVideoImpl::setVisibility`
- the CoreFoundation dictionary/array/property-list primitives used by the cluster;
- `AirPlayReceiverSessionScreen_Create`
- `AirPlayReceiverSessionScreen_Delete`
- `AirPlayReceiverSessionScreen_Setup`
- `AirPlayReceiverSessionScreen_SetTimeSynchronizer`
- `AirPlayReceiverSessionScreen_SetSecurityInfo`
- `AirPlayReceiverSessionScreen_UpdateState`
- `AirPlayReceiverSessionScreen_StartSession`
- `AirPlayReceiverSessionScreen_ProcessFrames`
- `AirPlayReceiverSessionScreen_StopSession`
- `AirPlay_DeriveAESKeySHA512ForScreen`
- `ServerSocketOpen`
- `SocketAccept`
- `NetSocket_CreateWithNative`
- `NetSocket_Delete`
- the DIO server-property callback above;
- additional HID and iAP2 functions.

### Type-111 handler

The cluster's main secondary-screen setup routine starts around `0x3c60`.

The disassembly contains an explicit stream loop:

```
stream[i] -> type
       |
       +-- 110 -> mark/retain normal stream
       |
       +-- 111 -> retain secondary descriptor
```

The 111 comparison is literal ARM code:

```
0x3d18: cmp r1,#0
0x3d1c: cmpeq r0,#111
```

After the 111 descriptor is retained, the same routine proceeds through the secondary SessionScreen construction and security/listener sequence.

The later portion has directly identified native MU0678-equivalent operations:

```
0x3f54 -> SessionScreen_Setup
0x3f7c -> SessionScreen_UpdateState
0x405c -> ServerSocketOpen
```

and the routine subsequently reaches the accept/secondary-worker path already mapped in the earlier audit.

### Runtime patch boundary

The cluster also has a separate runtime patching routine around `0x6200`.

At `0x6294`, it calls `dlsym` for the DIO `s_serverCopyPropertyCallback` symbol. It then derives the containing executable base and writes/validates code/data at calculated addresses.

This establishes that the Q solution uses **runtime interposition against the running executable/loaded libraries**, while preserving the original files on disk.

The MU0678 replica therefore needs two layers:

1. **Q-style runtime patch/interposition layer**, translated to MU0678 callback/code addresses and instruction encodings.
2. **Q cluster 111 implementation**, translated to MU0678 SessionScreen/security/socket/NvSS ABI.

The old `libairplay_patched_v6.so` remains useful only as a separate static reference for MU0678 instruction addresses/branch encoding. It should no longer be described as unrelated to the Q architecture: it is conceptually the same *kind* of runtime/code interposition, but the authoritative Q package implements that interposition from an external DIO cluster rather than shipping a modified `libairplay.so` file.
