# Evidence register

## Proven

1. `111` is the alternate stream.
2. Q creates a second SessionScreen.
3. Q calls UpdateState and ScreenSetup on B.
4. Q passes CreateResult as ScreenSetup arg 3.
5. Q derives B security with the stock AES/SHA512 helper.
6. Q opens a second listener and advertises its port as 111 dataPort.
7. Q accepts the second socket and calls ProcessFrames(B,NetSocket).
8. Q ScreenStreamCreate hook reads `(*arg1)+0x38`.
9. Q DIO callback uses `AirPlayReceiverSessionScreenSetDelegate`.
10. Q resolves the native DIO Screen creation/registration API surface.
11. Q hooks OMX lifecycle.
12. MHI2 stock processing ends in NvssVideoImpl::decode.

## Unresolved

- semantic meaning of `(*arg1)+0x38`;
- exact stock MHI2 ServerSocketOpen implementation/ABI;
- exact stock MHI2 equivalent of the Q SessionScreen-created callback;
- complete call-by-call proof of the Q native Screen creation/registration invocation chain;
- exact teardown ordering across secondary stop/delete and original AirPlay teardown;
- successful complete live 111 run through secondary decode/display.
