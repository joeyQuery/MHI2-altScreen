# MU0678 guarded-interposition audit — 2026-09-27

## Authoritative reference

Reference package:

`install.zip`

SHA-256:

`69a186d31b320b922e536581fb2f3fe4bdce2517c7dae34fbd8ae6b0dd9bd60b`

The actual MU0678 target executable supplied for this audit is:

`dio_manager`

SHA-256:

`0ca317e0549ffea6e6ce0ef77cd3f02b8c9d9bb2b2924c5c7e39f382f59ba29a`

Repository target blob remains:

`1f9dd87e2553f3a89fe083466d1ed3729dd9d094`

## Q patch primitive

The Q cluster's guarded primitive is at `0x47f8`.

It accepts:

- target address;
- expected/current word;
- replacement word;
- optional alternate expected word.

It reads the target, accepts either the expected word or already-installed replacement, otherwise optionally accepts the alternate guard, then uses `mprotect`, writes one 32-bit word, executes `dmb sy`, and returns success/failure.

The twelve callsites are:

```
0x6420  0x6454  0x64a0  0x64d4
0x6508  0x653c  0x6570  0x65ac
0x65e8  0x6624  0x6660  0x669c
```

## Operation 1 — closed

Q callsite `0x6420` targets:

```
s_serverCopyPropertyCallback + 0x4d784
= 0x1a5e04 in the Q DIO image
```

The Q ELF relocation table identifies that address as:

```
R_ARM_JUMP_SLOT  HIDRegisterDevice
```

The Q replacement word is the first instruction of the Q cluster hook at `0x53a4`:

```
e92d47f0
```

The MU0678 ELF relocation table identifies its native GOT slot as:

```
0x19d734  R_ARM_JUMP_SLOT  HIDRegisterDevice
```

The current MU word is:

```
0x115288
```

which is the MU0678 PLT resolver entry.

Therefore operation 1 has an exact target-side translation:

```
MU target:   0x19d734
guard word:  0x115288
replacement: MU cluster HID hook address
```

The replacement address itself must be the MU cluster's implementation of the Q HIDRegisterDevice hook; Q absolute `0x53a4` must not be copied.

## Operation 2 — closed

Q callsite `0x6454` targets:

```
s_serverCopyPropertyCallback + 0x4d9ac
= 0x1a602c in the Q DIO image
```

The Q ELF relocation table identifies:

```
R_ARM_JUMP_SLOT  HIDDeregisterDevice
```

MU0678 has:

```
0x19d7ec  R_ARM_JUMP_SLOT  HIDDeregisterDevice
```

and the current word is again:

```
0x115288
```

Therefore operation 2 has the exact MU target:

```
MU target:   0x19d7ec
guard word:  0x115288
replacement: MU cluster HIDDeregisterDevice hook address
```

Again, the Q replacement address is not portable.

## Critical interpretation of operations 3–12

The remaining Q writes are materially different.

The Q patch setup derives a runtime pointer from an internal cluster global at `0x10364` and then constructs several target addresses from that runtime object:

```
target 3  = runtime_object + 0xb8e80
target 4  = runtime_object + 0xb90a0
target 5  = runtime_object + 0xb9910
target 6  = runtime_object + 0xb9ce0
target 7  = runtime_object + 0xb8bc8
```

The final five targets are obtained indirectly through cluster globals:

```
*[0x102fc]
*[0x102d0]
*[0x102c4]
*[0x10280]
*[0x10244]
```

The expected values used by these calls resolve to Q runtime function/object state including:

```
ScreenStreamCreate
AirPlayReceiverSessionTearDown
AirPlayCopyServerInfo
ScreenStreamCreate
OmxVideoImpl::initialize
plus five cluster-owned hook-state slots
```

These are not fixed MU0678 DIO addresses.

The Q cluster ELF has a deliberately very large BSS region (`__bss_end__ = 0x81e6b4`), and the target derivation is through that runtime object/global state. Consequently, treating `+0xb8e80`, `+0xb90a0`, etc. as offsets from the MU0678 DIO image or from `s_serverCopyPropertyCallback` would be incorrect.

## Consequence for the MU implementation

The correct MU0678 translation is therefore:

1. reproduce the **host-level** guarded interposition where the Q patch demonstrably modifies host DIO linkage;
2. translate the Q-owned runtime hook/object state by behaviour, not by copying Q BSS offsets;
3. retain the already-proven direct MU0678 SessionScreen/Socket/NvSS implementation for the actual 111 path;
4. never fabricate MU addresses for the Q-owned object offsets.

The two host GOT translations above are now concrete.

The remaining ten operations are **not evidence for ten additional MU0678 DIO patch locations**. They belong to the Q cluster's own runtime state/hook machinery and must not be converted into bogus MU absolute addresses.

## MU target callback anchors

The real MU0678 DIO executable contains:

```
s_serverCopyPropertyCallback       0x139a88
s_sessionCreatedCallback           0x138fb4
s_sessionModesChangedCallback      0x139268
s_sessionFinalizedCallback         0x139424
s_sessionScreenCreatedCallback     0x1362d0
```

These remain the correct host interposition anchors.

## Status

Closed statically:

- Q package identity;
- guarded patch primitive;
- all twelve Q callsites;
- exact MU0678 GOT target for Q operation 1;
- exact MU0678 GOT target for Q operation 2;
- distinction between host GOT interposition and Q-owned runtime hook state;
- no justification for inventing MU translations for Q BSS offsets.

Still requiring implementation/runtime work:

- implement MU-native HID hook replacements if those two Q host hooks are retained for exact functional parity;
- bind the 111 SETUP/session callback into `mu0678_alt111_create()`;
- connect the 111 response to the MU secondary listener;
- connect secondary encoded access units to the MU output path;
- runtime validation of simultaneous 110 + 111.

This audit supersedes any repository text that describes all twelve Q patch calls as twelve MU0678 DIO patch sites.
