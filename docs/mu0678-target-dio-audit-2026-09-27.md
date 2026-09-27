# MU0678 target DIO audit — 2026-09-27

The target executable is present in the authoritative MU0678 dump repository and has now been inspected directly as an ELF32 ARM binary via its GitHub blob.

Path:
`dump/MU0678-MHI2_ER_AUG22_P3241-JTB-07429.06.161184010D/advanced/MU0678-appimg/eso/bin/apps/dio_manager`

Git blob SHA:
`1f9dd87e2553f3a89fe083466d1ed3729dd9d094`

Size:
`649413` bytes.

## ELF layout

- ELF32 little-endian ARM EABI5.
- Four program-load/runtime segments plus PT_DYNAMIC.
- RX LOAD: file offset 0, virtual address 0x100000, filesz 624244.
- RW LOAD: file offset 624244, virtual address 0x19a074, filesz 20000.
- Dynamic section: file offset 639664, virtual address 0x19d310, size 376.
- `.dynsym`: offset 8784, size 17680, entry size 16.
- `.dynstr`: offset 26464, size 50427.
- `.text`: virtual address 0x1151d0, size 449452.
- `.got`: virtual address 0x19d3c8, size 3856.

## Exact exported MU0678 callback anchors

- `s_serverCopyPropertyCallback` = `0x139a88`
- `s_sessionCreatedCallback` = `0x138fb4`
- `s_sessionModesChangedCallback` = `0x139268`
- `s_sessionFinalizedCallback` = `0x139424`
- `s_sessionScreenCreatedCallback` = `0x1362d0`
- `s_sessionRequestUICallback` = `0x1393c4`
- `s_sessionCopyProperty` = `0x138ef0`
- `main` = `0x116394`

Relative to the server-property callback, the MU0678 callback deltas are:

- sessionCreated: `-0xad4`
- sessionModesChanged: `+0x7e0`
- sessionFinalized: `+0x39c`
- sessionScreenCreated: `-0x2db8`

These are target-image facts. Q's callback deltas must not be copied.

## Existing host API surface

The same executable exports/imports the native Screen/AirPlay host surface needed by the external cluster, including:

- `ScreenCreate`
- `ScreenRegister`
- `ScreenRegisterDelegates`
- `AirPlayReceiverSessionScreenSetDelegate`
- `AirPlayReceiverSessionChangeModes`
- `AirPlayReceiverSessionSetDelegate`
- `AirPlayReceiverServerCreate`
- `AirPlayReceiverServerSetDelegate`

The AirPlay SessionScreen implementation itself remains in stock `/eso/lib/libairplay.so`.

## Correct implementation boundary

The target is therefore not missing. The remaining task is to reproduce the Q loader's runtime-interposition mechanism against this exact MU0678 executable:

1. load the external cluster into the DIO process;
2. resolve `s_serverCopyPropertyCallback` as the executable-layout anchor;
3. derive the MU0678 callback addresses using the target-specific deltas above;
4. install guarded MU0678-specific interception at the SETUP/session callback boundaries;
5. leave stock `libairplay.so` unchanged;
6. execute the already-closed secondary SessionScreen/111 worker/output path.

The exact 12 Q patch sites are not copied to MU0678. They are Q-loader-specific runtime patch locations. MU0678 sites must be derived from this executable's actual instructions and callback control flow.

## Important correction

Earlier repository text said the target MU0678 DIO executable was not available. That is superseded by this audit: the exact target binary is present in the dump repository and its ELF/symbol table has now been directly verified.
