# Trace 018 — MU0678 SessionScreen → native Screen association

## Decisive result

The remaining MHI2 SessionScreen association is now resolved.

There is **no hidden pointer from AirPlayReceiverSession to a native DIO Screen** that selects the output.

The stock association occurs in AirPlayReceiverSessionScreen_StartSession @ 0x25fd0.

### Stock flow

AirPlayReceiverSessionScreen_StartSession(screen, config)
  -> ScreenStreamCreate(config, screen + 0x1f4)
  -> obtain the native main Screen
  -> Copy/Get native Screen properties
  -> write those properties into ScreenStream
  -> ScreenStreamStart
  -> NvSS

The stream receives widthPixels, heightPixels, maxFPS, windowWidth, windowHeight, xOffset, yOffset and platformLayer from the selected native Screen.

## Critical consequence

The stock path selects the native **main Screen**, not a Screen through the AirPlay stream type.

Therefore 111 -> SessionScreen B does not automatically imply SessionScreen B -> native Screen[1].

There is no such association in the stock MU0678 code.

The native Screen subsystem is multi-object capable through ScreenCreate, ScreenRegister, ScreenDeregister, ScreenCopyMain and gScreenArray, but stock StartSession asks for the main Screen when constructing/configuring the stream.

## Replica consequence

Primary 110:
SessionScreen A -> stock StartSession -> main Screen -> ScreenStream A -> NvSS

Secondary 111:
SessionScreen B -> explicit native Screen selection -> ScreenStream B -> NvSS B

A secondary Screen object alone is insufficient. The secondary stream must explicitly receive the selected Screen's geometry/output properties.

Two valid implementation shapes remain:
1. a secondary-specific StartSession equivalent that clones properties from the selected native Screen;
2. a controlled secondary-only change to the Screen selection used by the stream.

Global modification of ScreenCopyMain is not acceptable because it would redirect the 110 path.

## Negative result

ScreenStreamCreate does not itself establish the physical display association. The stock StartSession path obtains the native Screen and then clones its properties into the stream.

Therefore the earlier hypothesis that ScreenStreamCreate +0x38 itself identifies the physical destination is not supported by the MU0678 static path.

## Final static SessionScreen state

The final static MHI2 boundary is now complete: the primary SessionScreen owns the stock primary stream; a secondary SessionScreen can be independently created, but its native output Screen must be explicitly selected and its properties copied into the secondary stream.

Remaining work is implementation/runtime: select the intended VC/KOMBI Screen, feed B's stream with its properties, prove changeModes=111, accept the secondary connection, and prove ProcessFrames(B) -> ScreenStream B -> NvSS -> VC/KOMBI.