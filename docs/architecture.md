# AltScreen architecture

The working MHI2Q implementation is a second AirPlay display/session path rather than a modification of the primary display. The audited Q type-111 path is the functional reference; see `docs/mhi2q-reference-package-2026-09-27.md`.

```
AirPlay Session
  ├── SessionScreen A / 110
  │      └── normal MMI path
  └── SessionScreen B / 111
         ├── DIO delegate/native Screen relationship
         ├── secondary listener
         ├── ProcessFrames(B,socket)
         └── secondary decoder/output integration
```

Important Q correction: although `AirPlayReceiverSessionScreen_StartSession` is dynamically resolved, the proven type-111 setup path does **not** call it. Do not model B as a simple copy of stock MHI2 110 StartSession.

AirPlay SessionScreen, DIO/native Screen, and ScreenStream are separate object layers.

MHI2 stock video flow:

```
ProcessFrames -> ScreenStreamProcessData -> ScreenStream/_ScreenStreamDecodeAll -> NvssVideoImpl::decode
```

The MHI2 implementation must preserve 110 and build the secondary 111 consumer around the Q-proven boundaries rather than replacing the primary path.
