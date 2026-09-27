# Replica plan

1. Preserve 110 and detect 111.
2. Create B and UpdateState(B,1).
3. Pass the real CreateResult to ScreenSetup.
4. Preserve Q time/security ordering.
5. Reconstruct the Q ServerSocketOpen ABI on MHI2.
6. Preserve 111/dataPort response and secondary accept.
7. Reproduce Q ScreenStreamCreate post-create state capture.
8. Reproduce SessionScreen-created DIO delegate binding and native Screen registration.
9. Translate OMX lifecycle to MHI2 NvSS only at the decoder primitive.
10. Reproduce Q teardown ordering.
11. Add Q safety/fingerprint controls only after the functional replica works.
