# Replica plan

The functional reference is the audited MHI2Q type-111 path in `docs/mhi2q-reference-package-2026-09-27.md`.

1. Preserve 110 and detect 111.
2. Create B and call `UpdateState(B,1)`.
3. Pass the real CreateResult to `ScreenSetup`.
4. Preserve Q time/security ordering using MU0678-native structure offsets and negotiated-value representation.
5. Reconstruct the **MU0678** secondary `ServerSocketOpen` ABI; do not copy Q's seven-argument register/stack layout.
6. Preserve the Q-proven 111/dataPort response and secondary accept.
7. Wrap the accepted native socket with the MU0678 equivalent of `NetSocket_CreateWithNative`.
8. Call `ProcessFrames(B,socket)`.
9. Reproduce the Q SessionScreen-created DIO delegate relationship once the external dispatcher is proven.
10. Map Q's native ScreenCreate/RegisterDelegates/Register surface to the exact MU0678 equivalent.
11. Translate Q OMX video operations to the MU0678 NvSS decoder primitive only; do not assume the surrounding Q object layout.
12. Reproduce Q teardown ordering where proven.
13. Keep the stock 110 path unchanged throughout.
14. Add Q safety/fingerprint controls only after the functional replica works.

### Explicitly not required by current evidence

- Do not add `StartSession(B)` merely because Q resolves the symbol.
- Do not reproduce Q's `ScreenStreamCreate` `(*arg1)+0x38` capture unless a later xref proves it is functionally required.
