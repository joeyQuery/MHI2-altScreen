# Secondary SessionScreen lifecycle

Q reference sequence:

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
15. ScreenStreamCreate hook captures Q state;
16. Q's DIO/decoder integration operates around the secondary stream; the exact cross-subsystem call ordering is still being traced;
17. teardown includes the secondary stop/delete path alongside the original AirPlay teardown; exact final ordering remains to be established.

The replica must preserve the proven Q ordering and must not invent ordering for the unresolved portions.
