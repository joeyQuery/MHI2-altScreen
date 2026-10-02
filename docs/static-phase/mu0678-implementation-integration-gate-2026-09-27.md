# MU0678 implementation integration gate — 2026-09-27

## What is implemented offline

The new src/mu0678/ cluster core implements:

1. Dynamic resolution of the existing AirPlay SessionScreen APIs.
2. Second SessionScreen creation.
3. UpdateState(B,1).
4. Setup(B,111-stream,CreateResult).
5. Stock MU0678 AES derivation boundary: AirPlay_DeriveAESKeySHA512ForScreen(session+0x1c8,16,negotiated64).
6. SetSecurityInfo(B,keyA,keyB).
7. MU0678-native post-accept ProcessFrames(B,NetSocket,[session+0xc]+0xc8), with stock StartSession(B) deliberately bypassed because MU0678 has one native Screen.
9. Dedicated local TCP output sink on 127.0.0.1:15557.
10. Q-compatible 32-byte AAF1 framing.
11. Explicit time-synchronizer entry point.
12. Teardown of duplicate SessionScreen state.

## What cannot safely be fabricated

The exact target MU0678 DIO executable is present in the repository dump and has now been directly inspected as ELF32 ARM. Its verified callback anchors are documented in docs/mu0678-target-dio-audit-2026-09-27.md. The remaining loader/interposition work is therefore target-image derivation against a known executable, not a missing-input problem.

## Output

The Q helper proves loopback endpoint 127.0.0.1:15557, a 32-byte AAF1 header, byte 4 = 2, byte 7 = 32, and payload immediately following the header.

MU0678 must keep its own display geometry and routing. The recovered logical target is DISPLAYABLE_KOMBI_MAP_VIEW, ID 33, 1440x455. The physical route remains a hardware fact.

## Runtime gate

No static work can prove iPhone 111 negotiation, receipt by the secondary listener, independent decode from 110, physical DMDT routing, or stable reconnect/teardown. Those remain hardware validation gates after the target-specific interposition is installed.
