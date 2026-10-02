# MU0678 implementation closure — 2026-09-27

The supplied `install.zip` remains the authoritative working reference:

`69a186d31b320b922e536581fb2f3fe4bdce2517c7dae34fbd8ae6b0dd9bd60b`

## Implemented

The MU0678 cluster now implements the reference architecture at the external DIO boundary:

- stock `libairplay.so` remains the on-disk library;
- the MU0678 `dio_manager` is patched only to add `libmhi2qcarplaycluster.so` as a DT_NEEDED dependency;
- the cluster is actually staged by the package script;
- the cluster links the QNX socket dependency expected by the reference;
- DIO `s_serverCopyPropertyCallback` and `s_sessionFinalizedCallback` are guarded runtime interposition points;
- installation is rollback-safe if the expected MU0678 DIO/AirPlay layout is not present;
- SETUP type 111 is intercepted at the recovered MU0678 branch;
- the negotiated 64-bit value is obtained from the MU0678 stream descriptor;
- the MU0678-native SessionScreen B security derivation uses `session+0x1c8`;
- the MU0678-native secondary listener uses the recovered screen/media `ServerSocketOpen` ABI;
- the 111 response advertises type 111 and its allocated data port;
- the dedicated worker performs `SocketAccept -> NetSocket_CreateWithNative -> ProcessFrames(B, socket, [session+0x0c]+0xc8)`; the MU0678 stock `StartSession(B)` call is deliberately omitted because its singleton `ScreenCopyMain` binding would rebind the primary output;
- the Q-compatible 32-byte AAF1 live-CarPlay transport is emitted from the secondary `ScreenStreamProcessData` boundary;
- the output writer gates transmission until codec configuration and an IDR access unit are available;
- the exact reference `mhi2q-aa-cluster-display --live-carplay` helper is still used by the package;
- teardown stops the secondary worker, stream, SessionScreen and output transport independently.

## Reference-specific corrections

The implementation deliberately does not copy:

- Q absolute addresses;
- Q listener constants;
- Q OMX object offsets;
- Q's `session+0x1c8` ProcessFrames context offset;
- Q's 1440x540 geometry;
- Q's DMDT context arguments.

MU0678 uses its proven native equivalents instead.

## Display-routing boundary

The MU0678 firmware proves that the stock AirPlay StartSession path obtains the single main native Screen. It does not contain a second native Screen factory.

Therefore a literal copy of the QNX helper's physical DMDT route cannot be claimed as a MU0678 equivalent. The implementation keeps the secondary encoded stream independent and bypasses MU0678's singleton native Screen/NvSS binding. The exact reference-equivalent downstream physical VC route remains hardware-specific.

The recovered logical target remains:

`DISPLAYABLE_KOMBI_MAP_VIEW`, ID 33, 1440x455.

The repository does not fabricate a physical UUID/DSI mapping.

## Runtime gates

Static work cannot prove:

1. the iPhone actually sends a 111 stream against this MU0678 advertisement;
2. the secondary SessionScreen can be started without disturbing the stock main Screen on this exact firmware;
3. the physical VC/KOMBI display route;
4. stable reconnect/teardown on hardware.

Those are hardware/runtime validation gates, not missing static architecture.
