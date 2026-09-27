# AirPlay stream 111

- `110` = normal/main CarPlay stream.
- `111` = alternate/secondary CarPlay stream.

`altScreen` is an AirPlay feature-negotiation capability, not an iAP2 Identify capability.

`111` is a stream selector, not the complete screen-description object. Dimensions/UUID/EDID and related data occur elsewhere in `/info` and SETUP/session structures.

On iOS 23G71, CarPlayDisplayUtils has primary/secondary ScreenType and DisplaySizes has `screenType` at `+0x00`; secondary is represented as `1`. This does not prove direct population from `/info`.
