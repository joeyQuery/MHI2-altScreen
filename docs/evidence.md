# Evidence register

## Proven

1. `111` is the alternate stream.
2. Q creates a second SessionScreen.
3. Q calls UpdateState and ScreenSetup on B.
4. Q passes CreateResult as ScreenSetup arg 3.
5. Q derives B security with the stock AES/SHA512 helper.
6. Q opens a second listener and advertises its port as 111 dataPort.
7. Q accepts the second socket and calls ProcessFrames(B,NetSocket).
8. Q ScreenStreamCreate hook reads `(*arg1)+0x38`; the captured value is not read elsewhere in the Q cluster, so this is not proven to be a functional dependency.
9. Q DIO callback uses `AirPlayReceiverSessionScreenSetDelegate`.
10. Q DIO loader contains executable ScreenCreate/ScreenRegisterDelegates/ScreenRegister callsites.
11. Q resolves StartSession but the proven type-111 setup path does not call it.
12. Q hooks OMX lifecycle.
13. MHI2 stock processing ends in NvssVideoImpl::decode.
14. Stock MU0678 ServerSocketOpen implementation is at `0x79910` and consumes r0/r1/r3 plus stack arguments 14/15/16; incoming r2 is not consumed as an input.

## Unresolved

- exact semantic values/stack construction for stock MHI2 ServerSocketOpen arguments 1/2/4/14/15/16;
- exact external dispatcher that invokes the Q SessionScreen-created callback;
- exact teardown ordering across secondary stop/delete and original AirPlay teardown;
- successful complete live 111 run through secondary decode/display.
