# Trace 011 — Stock MHI2 ServerSocketOpen Callers

## Evidence

Static callsite analysis of stock MU0678 libairplay.so found eight direct calls through the ServerSocketOpen PLT entry:

- 0x23380 — inside AirPlayReceiverSessionSetup
- 0x2373c — inside AirPlayReceiverSessionSetup
- 0x23a94 — inside AirPlayReceiverSessionSetup
- 0x23c48 — inside AirPlayReceiverSessionSetup
- 0x5e328 — inside IPCAgent_DeleteAsync
- 0x747e0 — inside NetPerfCreate
- 0x79c6c — inside ServerSocketPairOpen
- 0x79c90 — inside ServerSocketPairOpen

The last two are internal calls from the exported ServerSocketPairOpen implementation, showing that ServerSocketOpen is part of a broader socket utility rather than a single-purpose CarPlay listener primitive.

Within AirPlayReceiverSessionSetup, the calls use different register/stack preparations, including distinct values in the second and third argument registers. Therefore there is not one universal semantic interpretation that can safely be inferred from a single callsite.

## Conclusion

The MHI2 ABI mismatch is real and the native caller population is now mapped.

What remains unresolved is the semantic meaning of arguments 1/2/4/14/15/16 for the specific secondary CarPlay listener. Those meanings must be established by following the relevant setup branch and its socket helper state, not by copying a register value from an unrelated ServerSocketOpen caller.
