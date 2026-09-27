# MHI2Q patch map

| Stock callsite | Replacement |
|---:|---:|
| `0x6420` | `0x53A4` |
| `0x6454` | `0x21A0` |
| `0x64A0` | `0x3C60` |
| `0x64D4` | `0x33F0` |
| `0x6508` | `0x48C4` |
| `0x653C` | `0x4710` |
| `0x6570` | `0x46D4` |
| `0x65AC` | `0x1954` |
| `0x65E8` | `0x19B8` |
| `0x6624` | `0x1C04` |
| `0x6660` | `0x1B8C` |
| `0x669C` | `0x19F4` |

Functional wrappers include SETUP (`0x3C60`), TearDown (`0x33F0`), `/info` (`0x48C4`), ScreenStreamCreate (`0x4710`), ScreenSetup (`0x46D4`), and OMX lifecycle hooks (`0x1954`,`0x19B8`,`0x1C04`,`0x1B8C`,`0x19F4`).

Q patch primitive: `0x47F8`. It validates expected bytes and synchronizes modified code/cache state. Internal fingerprint is anchored from resolved `AirPlayReceiverSessionScreen_Create`.

Production Q stock-libairplay SHA256: `01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2`.
