# Corrected interpretations

- `screenType` secondary=`1` is proven; direct population from AirPlay `/info` is not.
- Q replacement addresses are the function entry points themselves; not `+4`.
- Q internal fingerprint anchor is the resolved AirPlayReceiverSessionScreen_Create pointer; not the DIO callback.
- Q ScreenStreamCreate state access is `(*arg1)+0x38`; not return-value `+0x38`.
- MU0678 SetTimeSynchronizer copies only the first 12-byte pointer payload; earlier G1/G2 interpretation was overextended.
