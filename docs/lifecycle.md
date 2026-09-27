# Secondary SessionScreen lifecycle

Q reference sequence proven for the type-111 setup path:

1. inspect SETUP streams;
2. detect `type == 111`;
3. create SessionScreen B;
4. `UpdateState(B,1)`;
5. `Setup(B,stream111,CreateResult)`;
6. `SetTimeSynchronizer(B,q_time_state,...)`;
7. obtain negotiated 64-bit value;
8. `DeriveAESKeySHA512ForScreen(session+0x1c0,16,value)`;
9. `SetSecurityInfo(B,keyA,keyB)`;
10. open second listener;
11. add `{type:111,dataPort:N}`;
12. accept secondary connection;
13. `NetSocket_CreateWithNative`;
14. `ProcessFrames(B,socket)`;
15. DIO/decoder integration around the secondary stream;
16. teardown includes secondary stop/delete alongside original AirPlay teardown; exact final ordering remains unresolved.

### Important correction

Q resolves `AirPlayReceiverSessionScreen_StartSession`, but the traced type-111 setup block contains **no call through that pointer**. Do not insert a synthetic `StartSession(B)` into the replica.

The ScreenStreamCreate `(*arg1)+0x38` capture is also not proven functionally required; the captured destination is not read elsewhere in the Q cluster.

The replica must preserve proven Q ordering and must not invent ordering for unresolved portions.
