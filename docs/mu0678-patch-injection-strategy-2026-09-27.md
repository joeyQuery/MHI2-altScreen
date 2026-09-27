# MU0678 patch injection strategy — 2026-09-27

## Scope correction

This document describes the **supplied MU0678 binary patch as an injection-mechanism artifact only**.

It is **not** the functional reference for the working MHI2Q implementation.

The authoritative functional reference is the supplied Q `install.zip`. Its installer explicitly verifies the existing `/eso/lib/libairplay.so` and does not replace it. The Q implementation instead uses a DIO/CarPlay loader plus `libmhi2qcarplaycluster.so` and existing AirPlay APIs.

Therefore:

- do not infer the Q architecture from the MU0678 patched `libairplay.so`;
- do not treat `libairplay_patched_v6.so` as the working implementation;
- do not deploy this binary merely because it reproduces the supplied patch byte-for-byte;
- use this artifact only when deciding whether a MU0678 external-loader implementation needs an executable injection mechanism.

## Supplied MU0678 patch evidence

Reference artifacts:

- pristine baseline: `libairplay.so`
- stock SHA-256: `a3ab0fe0ac20a712f7f240c1abd852a38faf78f2673880342d0eb24a6d8c29be`
- supplied patch artifact: `libairplay_patched_v6.so`

The supplied patch:

- preserves the existing RX LOAD;
- appends a page-aligned payload at file offset `0xd2000`;
- maps it at virtual address `0xac000`;
- adds a fifth PT_LOAD;
- uses flags `0x7` (RWE);
- uses `0x1000` alignment;
- has payload/filesz/memsz `0x428`;
- redirects selected stock ARM instructions into the injected payload;
- returns to stock continuation addresses.

The known branch sites are:

- `0x20348`
- `0x214d0`
- `0x23260`
- `0x2569c`
- `0x2602c`
- `0x7fd9c`

The critical stock 111 boundary is:

```
0x2325c: cmp r0,#110
0x23260: bne 0x231f0
```

The supplied patch redirects that branch into its appended payload, which begins with a `cmp r0,#111` dispatcher.

This proves how that particular MU0678 patch injected code into `libairplay.so`. It does **not** prove that this is how the working Q implementation works.

## Correct MU0678 implementation direction

The Q reference says the preferred architecture is external to `libairplay.so`:

```
stock libairplay.so
      ^
      | existing exported ABI
      |
custom /eso/bin/apps/dio_manager
      |
      +-- custom CarPlay cluster
      +-- secondary SessionScreen state
      +-- secondary transport
      +-- output helper
```

The immediate MU0678 question is therefore whether the stock MU0678 DIO manager can be replaced or wrapped with a custom DIO loader while preserving its existing AirPlay/DIO ABI.

That question must be answered from the actual MU0678 `dio_manager` binary and startup/linkage configuration before selecting binary injection as the implementation mechanism.

## Status

The binary patch itself is reproducible, but its functional role in the working Q solution is disproven.

The next implementation gate is a MU0678 DIO-loader/cluster prototype using the stock MU0678 `libairplay.so`, followed by hardware validation.

