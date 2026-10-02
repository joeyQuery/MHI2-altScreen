# GEN2 development source

> The source in this directory is intentionally **newer than the promoted experimental binary**.

Promoted vehicle-tested checkpoint:

```text
artifacts/mu1440/altscreen111/experimental-gen2-2026-09-25/libaltscreen111.so
SHA-256:
f3efa9f09972422307f1b9e37e323539036d935f9cc630a1ed7d8f223a2bbae2
```

That older binary remains useful because its behavior and failure mode are well understood.

The source here tracks the current development line so contributors can help with the actual open
problem rather than rebuilding already-completed work from the older checkpoint.

## What is newer in this source

The current development source includes work in these areas.

### PlatformControl / SessionControl flight recording

The hook records the real CFString command object and bounded/redacted parameter structures around
the stock handler.

This supports correlation of:

- `suggestUI`
- `showUI`
- `stopUI`
- `forceKeyFrame`
- `changeModes`
- `modesChanged`
- stream descriptors including Type 111

without replacing the stock control-plane implementation.

### suggestUI receiver behavior

Stock MU1440 has no auxiliary-cluster presenter and returns a stock-only "not handled" result for
`suggestUI`.

When the project-owned private Type-111 control session is active, current development code can
acknowledge `suggestUI` while keeping the URL list as **candidate metadata**, rather than blindly
translating every suggestion into `showUI`.

That distinction follows both vehicle observations and public comparator behavior.

### Advertised cluster URL roles

The current source advertises:

```text
maps:/car/instrumentcluster
maps:/car/instrumentcluster/map
maps:/car/instrumentcluster/instructioncard
```

through `altScreenSuggestUIURLs`.

These are UI roles inside the same secondary-display session, not separate Type-111 streams.

### ViewArea / SafeArea metadata

Current development can advertise ViewArea/SafeArea metadata while deliberately avoiding
Audi-specific geometry.

The first Škoda baseline uses the complete 1010×376 secondary-display canvas as the conservative
starting point.

Dynamic in-session ViewArea switching is still research, not a promoted feature.

### Serialized recovery controller

Recovery commands are no longer treated as unrelated one-off diagnostics.

Current source serializes control operations so `forceKeyFrame` demand does not race with an
in-flight UI transaction.

The resync logic tracks:

- stream generation;
- codec/config generation;
- consumer generation;
- IDR state;
- request/retry/completion state.

### VideoConfig / consumer generation handling

A newly attached consumer must become valid through a coherent codec/config + IDR boundary.

Current source explicitly resets/re-primes the consumer when codec state requires it rather than
continuing to feed stale-generation frames.

### Stream 110 / 111 separation

The private Type-111 lifecycle is increasingly kept independent from unrelated stock main-screen 110
behavior.

This matters during provider/app transitions where the auxiliary stream may remain established while
other CarPlay state changes.

## Why there is no matching promoted binary yet

The development line is being used to classify several variables at once:

- UI candidate withdrawal/selection;
- navigation ownership;
- bitstream restart;
- config/IDR generation;
- provider handover.

Promoting every intermediate build would create a misleading pile of binaries.

The repository therefore uses this policy:

```text
src/      = current development work
artifacts = selected understandable checkpoints
```

A newer binary should be promoted only when its behavior is sufficiently characterized on vehicle
hardware.

## Build

Use:

```sh
bash tools/build_altscreen111_gen2.sh build/libaltscreen111.so
```

or the public GitHub Actions workflow:

```text
Build experimental GEN2 AltScreen
```

A CI-produced development binary is not automatically the new promoted vehicle baseline.

## Current engineering problem

See:

- `docs/status/CURRENT_DEVELOPMENT_STATUS.md`
- `docs/findings/KNOWN_ISSUES.md`
- `docs/research/IOS27_SENDER_LIFECYCLE.md`
- `docs/research/PLATFORMCONTROL_FLIGHT_RECORDER.md`
