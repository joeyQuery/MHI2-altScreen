# Corrected interpretations

- `screenType` secondary=`1` is proven; direct population from AirPlay `/info` is not.
- Q replacement addresses are the function entry points themselves; not `+4`.
- Q internal fingerprint anchor is the resolved AirPlayReceiverSessionScreen_Create pointer; not the DIO callback.
- Q ScreenStreamCreate state access is `(*arg1)+0x38`; not return-value `+0x38`.
- MU0678 SetTimeSynchronizer copies only the first 12-byte pointer payload; earlier G1/G2 interpretation was overextended.
- Q resolves `AirPlayReceiverSessionScreen_StartSession`, but the proven type-111 setup path does not call it; adding a synthetic `StartSession(B)` to the MHI2 replica is therefore unsupported by current Q evidence.
- Q's captured `(*arg1)+0x38` value is not subsequently consumed by the Q cluster; it remains diagnostic/unresolved rather than a proven functional dependency.
