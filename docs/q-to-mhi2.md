# Q to MHI2 functional mapping

| Q operation | Q evidence | MHI2 state | Replica action |
|---|---|---|---|
| detect 111 | SETUP scan | present | preserve |
| Create B | SessionScreen_Create | present | preserve |
| UpdateState | `UpdateState(B,1)` | present | preserve |
| ScreenSetup | `Setup(B,111,CreateResult)` | arg3 may be zero | pass CreateResult |
| time sync | Q local state | present | trace exact 12-byte payload |
| AES | stock Derive helper | present | preserve |
| security | SetSecurityInfo | present | preserve |
| listener | 7-arg ServerSocketOpen | simplified | reconstruct ABI |
| response | 111 + dataPort | present | preserve |
| accept | SocketAccept | present | preserve |
| socket | NetSocket_CreateWithNative | present | preserve |
| frame pump | ProcessFrames | present | preserve |
| ScreenStreamCreate | hook `0x4710` | different bookkeeping | reproduce capture |
| stream state | `(*arg1)+0x38` | missing | add after layout proof |
| SessionScreen delegate | callback -> SetDelegate | different path | reproduce relationship |
| native DIO Screen | Create -> RegisterDelegates -> Register | variants | map exact equivalent |
| decoder | OMX lifecycle | NvSS | translate primitive only |
| teardown | stop/delete | partial | reproduce ordering |
