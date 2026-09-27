# ELF byte audit — MU0678 libairplay type-111 patch (2026-09-27)

## Artifact identity

The Library contains these byte-identical 857727-byte artifacts:

- `libairplay.so.111`
- `libairplay_type111_accept.so`
- `libairplay_type111_accept_857727.so`
- `libairplay_type111_exp.so`

SHA-256: `1aa2add92b115f4bd1d19bdd34796e4a59a7b56cacfdf6a5df6c5500eca68133`

This is the active "type111 accept" patch artifact audited here.

## Stock reference recovered from repository evidence

The repository's audited stock disassembly records `AirPlayReceiverSessionSetup @ 0x22fd8` as:

```
0x23248  cmp r0, #100
0x23250  blo  0x231f0
0x23254  cmp r0, #101
0x23258  bls  0x23480
0x2325c  cmp r0, #110
0x23260  bne  0x231f0
...
0x233a0  mov r2, #110
```

Therefore stock admits 100, 101 and 110 and rejects 111.

The exact stock instruction bytes at the two modified instructions are:

- `0x2325c: e3 50 00 6e` = `cmp r0,#110`
- `0x233a0: e3 a0 20 6e` = `mov r2,#110`

The physical stock ELF is **not present in the current Library/repository artifact set**, so the full-file SHA-256 of the original MU0678 stock ELF cannot be independently recomputed here. The byte-level patch below is nevertheless exact against the documented stock instruction bytes and the recovered patched ELF.

## Definitive patch matrix

| File offset / VA | Stock | Patched | Meaning |
|---:|---|---|---|
| `0x2325c` | `e350006e` — `cmp r0,#110` | `e350006f` — `cmp r0,#111` | Raises accepted upper bound from 110 to 111 |
| `0x233a0` | `e3a0206e` — `mov r2,#110` | `e59d2080` — `ldr r2,[sp,#0x80]` | Response descriptor type becomes the requested stream type instead of hard-coded 110 |

All other bytes of the patched artifact are unchanged relative to the reconstructed stock image obtained by reversing exactly these two documented stock instructions. This establishes an 8-byte payload change: two ARM instructions, 4 bytes each.

## What the active patch actually does

After patching:

```
cmp r0,#111
bhi  0x231f0
```

This means the admission gate accepts:

- 100
- 101
- 102 through 111

The existing 110 path is therefore preserved, but the patch does **not** create a special exact-`111` branch. It widens the accepted range to 111.

At `0x233a0`, the response type is loaded from the value previously stored at `[sp,#0x80]`, i.e. the stream type returned by the preceding stream inspection. Thus a negotiated 111 stream can be returned as type 111 instead of being forced to 110.

## Alternate artifacts audited

### `libairplay_111_patch.so`

SHA-256: `7c9aa9298606a1cbcd2bfdc0785e967a4b9beb782814060017e173d11dd52e66`

Differs from `libairplay.so.111` at exactly 8 bytes:

- `0x2324c`: `str r0,[sp,#0x80]` -> `mov r8,r0`
- `0x23260`: `bhi` -> `bne`
- `0x233a0`: `ldr r2,[sp,#0x80]` -> `mov r2,r8`

This is an **exact-match 111 variant** rather than the active upper-bound variant.

### `libairplay_type111_clean.so`

SHA-256: `dee8f5b5199182e3f39a90b715c3e3c7fdbfd29620c1dc0ad96879658a340460`

Differs from the active artifact at only one byte/instruction condition:

- `0x23260`: active `bhi`; clean variant `bne`.

It therefore corresponds to the exact-match form while retaining the active artifact's stack-based type preservation.

### `libairplay_altscreen_111.so`

SHA-256: `059ffa24bda1803e0fc014918a82e10b22b545223a9212432e43d9c1ffc6d915`

This is not a byte-equivalent copy of the active patch. It changes 91 bytes across 20 runs, including the admission/response region and additional diagnostic/experimental sites.

### `libairplay_patched_v6.so`

SHA-256: `62eee8aacaad53be049f0e458abaf492c777da7879978a8c2320b661156954d0`

Size is 861384 bytes rather than 857727, so it is not the same in-place patch artifact and must be treated as a different ELF layout/build.

## Copy/change/omit/add conclusion

### Copied exactly into the active patch

The functional concept copied from the stock/Q reference is narrow:

1. preserve the existing stream-type value;
2. allow the alternate stream through SETUP;
3. return that negotiated type in the response descriptor.

The actual active ELF implementation is only the two instruction substitutions above.

### Changed

- Stock `cmp #110` -> patched `cmp #111`.
- Stock response `mov r2,#110` -> patched response type load from `[sp,#0x80]`.

### Omitted

No second SessionScreen creation, second ScreenStream creation, secondary listener, second socket accept, or secondary decoder/output binding is added by this ELF patch.

Those operations are outside this binary patch and are why the direct 111 patch can black-screen the original CarPlay path.

### Added

No new code section, function, relocation, or ELF growth is present in `libairplay.so.111`. The artifact remains exactly 857727 bytes.

The patch is therefore an **in-place 8-byte / two-instruction modification**, not an injected function or expanded ELF.

## Critical audit result

The current MHI2 AltScreen implementation must not describe `libairplay.so.111` as a complete second-screen implementation.

It is specifically a SETUP admission/response patch. The independent second SessionScreen + listener + ProcessFrames + native Screen/output binding is the separate functional work being replicated from MHI2Q.

The physical stock ELF should still be archived alongside this report before claiming a cryptographic full-file stock-vs-patched verification.