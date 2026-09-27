# AltScreen architecture

The working MHI2Q implementation is a second AirPlay display/session path rather than a modification of the primary display.

```text
AirPlay Session
  ├── SessionScreen A / 110
  │      └── normal MMI path
  └── SessionScreen B / 111
         ├── DIO delegate/native Screen
         ├── ScreenStream B
         ├── second decoder
         └── secondary listener
```

AirPlay SessionScreen, DIO/native Screen, and ScreenStream are separate object layers.

MHI2 stock video flow:

```text
ProcessFrames -> ScreenStreamProcessData -> ScreenStream/_ScreenStreamDecodeAll -> NvssVideoImpl::decode
```
