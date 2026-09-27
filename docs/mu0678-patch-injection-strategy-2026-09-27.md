# MU0678 patch injection strategy — 2026-09-27

## Decision

Use the **exact binary-injection mechanism and functional payload of the supplied original MU0678 patch**.

The supplied reference is:

- pristine baseline: `libairplay.so`
- stock SHA-256: `a3ab0fe0ac20a712f7f240c1abd852a38faf78f2673880342d0eb24a6d8c29be`
- original patched reference: `libairplay_patched_v6.so`
- patched SHA-256: `62eee8aacaad53be049f0e458abaf492c777da7879978a8c2320b661156954d0`

The user has explicitly authorized using that supplied patch as the functional reference.

## Exact reproduction result

The original patched ELF has been reproduced from the pristine MU0678 baseline.

The reproduction process:

1. verifies the pristine stock SHA-256;
2. verifies the supplied original-patch SHA-256;
3. applies every byte difference in the original file range;
4. appends the exact post-stock payload/program-header region;
5. verifies the resulting SHA-256 against the supplied patch.

The resulting file is **byte-identical** to the supplied `libairplay_patched_v6.so`.

A reproducible script is committed as:

`tools/reproduce-original-mu0678-patch.py`

## ELF mechanism

The original patch:

- preserves the existing RX LOAD;
- appends a page-aligned payload at file offset `0xd2000`;
- maps it at virtual address `0xac000`;
- adds a fifth PT_LOAD;
- uses flags `0x7` (RWE);
- uses `0x1000` alignment;
- has payload/filesz/memsz `0x428`;
- redirects selected stock ARM instructions into the injected payload;
- returns to stock continuation addresses.

The patched ELF still has 35 section headers; the new payload is represented through the new PT_LOAD rather than a new section.

## Exact hooks in the supplied patch

The supplied reference changes these stock locations:

- `0x20348`
- `0x214d0`
- `0x23260`
- `0x2569c`
- `0x2602c`
- `0x7fd9c`

The critical 111 dispatch is:

```
0x2325c: cmp r0,#110
0x23260: bne 0x231f0       ; stock
```

becoming:

```
0x2325c: cmp r0,#110
0x23260: bne 0xac000       ; injected dispatcher
```

The injected payload begins at `0xac000` and starts with:

```
cmp r0,#111
```

Thus the supplied patch's dispatcher preserves the ordinary path while routing the alternate stream into the injected implementation.

## Functional payload

Because the user has authorized exact replication of the supplied patch, the reference payload itself is now the functional implementation reference. It is not being replaced by a newly invented MU0678 design.

The payload contains the original patch's:

- type-111 dispatcher;
- persistent secondary state;
- SessionScreen creation/state setup;
- secondary listener;
- security/state setup;
- accepted-socket handling;
- secondary ScreenStream/session path;
- return trampolines;
- additional lifecycle hooks.

The payload's embedded addresses are MU0678-specific and are therefore reproduced exactly rather than translated from Q.

## Verification

The locally reproduced binary has:

- size: `861384` bytes;
- SHA-256: `62eee8aacaad53be049f0e458abaf492c777da7879978a8c2320b661156954d0`;
- 5 program headers;
- new PT_LOAD at file offset `0xd2000`;
- new PT_LOAD virtual address `0xac000`;
- new PT_LOAD flags `RWE`;
- new PT_LOAD size `0x428`.

A byte-for-byte `cmp` against the supplied `libairplay_patched_v6.so` succeeds.

## Important distinction

This establishes **exact reproduction of the supplied patch**, not successful hardware execution.

The patch has not been independently runtime-validated on the target MU0678 in this session. The next gate is therefore installation/runtime testing of this exact binary, followed by capture of the 110/111 lifecycle and simultaneous output.

The pristine `/mnt/data/libairplay.so` remains untouched.
