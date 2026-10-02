# MU0678 type-111 callback boundary — 2026-09-27

## Proven from the Q reference

The Q type-111 entry is at `libmhi2qcarplaycluster.so` `0x3c60`.

Its first instructions preserve:

- `r0` as the first argument,
- `r1` as the stream collection,
- `r2` as the setup-result object.

It immediately treats `r1` as a collection: count, indexed value lookup, and per-stream inspection. The dispatcher explicitly distinguishes stream type `110` and stream type `111`.

Therefore the replacement ABI is:

```c
hook(session, streams, setup_result)
```

The Q entry then creates the secondary SessionScreen, derives the secondary screen security state, opens/uses the second media path, and enters the Q worker.

## Proven MU0678 delegate ABI

The MU0678 DIO session-created callback builds the public AirPlay delegate and calls `AirPlayReceiverSessionSetDelegate`.

The public delegate layout is:

```text
+00 context
+04 initialize_f
+08 finalize_f
+0c started_f
+10 control_f
+14 copyProperty_f
+18 setProperty_f
+1c modesChanged_f
+20 requestUI_f
+24 duckAudio_f
+28 unduckAudio_f
```

The MU0678 loader allocates additional words beyond this public layout, but their semantics are not assumed.

## Exact MU0678 host ABI anchors

Verified imports in `dio_manager`:

- `0x19d4c4` — `AirPlayReceiverSessionChangeModes`
- `0x19d544` — `AirPlayReceiverSessionScreenSetDelegate`
- `0x19d734` — `HIDRegisterDevice`
- `0x19d7a0` — `AirPlayReceiverSessionSetDelegate`
- `0x19d7ec` — `HIDDeregisterDevice`

The native PLT resolver word at the two HID slots is `0x115288`.

## Implementation rule

Do **not** substitute a field from the public delegate for the Q `(session, streams, setup_result)` hook. The signatures are different.

The repository now contains a guarded MU0678 interposition primitive and derives the DIO image base from the exact mangled `s_serverCopyPropertyCallback` symbol. It does not install an unproven replacement ABI.

The remaining static task is to identify the MU-native callback/object slot that invokes the `(session, streams, setup_result)` operation and prove its old function pointer before changing it.

That is the only target-specific SETUP integration edge left open by the current evidence.
