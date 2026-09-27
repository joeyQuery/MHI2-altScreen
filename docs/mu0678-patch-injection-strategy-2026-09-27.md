# MU0678 patch injection strategy — 2026-09-27

## Decision

Use the same binary-injection mechanism demonstrated by the supplied original patched ELF.

The original mechanism is not an in-place code-cave implementation. It:

1. preserves the existing RX LOAD;
2. appends a page-aligned executable payload at file offset 0xd2000;
3. maps that payload at virtual address 0xac000;
4. adds a fifth program header of type PT_LOAD;
5. marks the new segment RWE;
6. redirects selected stock instructions with ARM branch instructions into the injected payload;
7. returns from the payload to the original continuation addresses.

The reference patched ELF has:

PT_LOAD offset   0xd2000
PT_LOAD vaddr    0xac000
PT_LOAD filesz   0x428
PT_LOAD memsz    0x428
PT_LOAD flags    RWE (0x7)
PT_LOAD align    0x1000

The stock ELF has four program headers; the patched reference has five.

## Why this is the chosen mechanism

The pristine MU0678 ELF does not contain a large executable cave suitable for the required handler. The demonstrated patch solves exactly that problem by extending the ELF with a new executable LOAD rather than consuming uncertain padding inside existing sections.

Therefore the MU0678 replica should use the same mechanism.

## Hook model

The payload is reached from stock code through ARM B instructions. The supplied patched ELF demonstrates this at:

- 0x20348
- 0x214d0
- 0x23260
- 0x2569c
- 0x2602c
- 0x7fd9c

The 111 SETUP interception is specifically:

0x2325c: cmp r0,#110       ; stock
0x23260: bne 0x231f0      ; stock continuation

patched:
0x2325c: cmp r0,#110       ; preserve
0x23260: bne 0xac000      ; enter injected dispatcher

The injected dispatcher begins by testing r0 == 111 and handles the alternate-screen path. The exact reference payload must not be copied as the functional implementation because it is an experimental implementation; only its ELF injection mechanism is being reproduced.

## New MU0678 patch construction

The final patch should therefore be constructed as:

stock libairplay.so
        |
        +-- modify selected ARM B instructions
        |
        +-- append page-aligned payload
        |
        +-- add PT_LOAD:
              file offset 0xd2000
              vaddr      0xac000
              flags      RWE
              size       = actual payload

The payload should contain:

- 111 SETUP dispatcher;
- persistent B-state handling;
- SessionScreen B lifecycle;
- MU0678-native security derivation;
- secondary listener;
- 111/dataPort response construction;
- accept/NetSocket wrapping;
- post-accept StartSession;
- ProcessFrames;
- dedicated output transport;
- teardown;
- return trampolines.

## Critical relocation rule

The injected payload must use absolute addresses only where the MU0678 image's existing relocation model makes that safe. The reference payload encodes MU0678-relative addresses directly because it is injected into a fixed firmware image.

Every branch/call target in the new payload must be calculated against the actual MU0678 virtual address map, not against the Q package.

## Reference-vs-replica rule

The supplied patched ELF establishes the mechanism:

- appended PT_LOAD;
- RWE permissions;
- page alignment;
- ARM branch redirection;
- return to original continuations.

It does not establish the correctness of its experimental 111 lifecycle. The audited Q package remains the functional reference for what the new payload must do.

## Current state

The pristine uploaded MU0678 baseline is unchanged.

No branch has yet been redirected in the user's uploaded file, and no patched binary is being represented as flash-ready until the new payload has been assembled and byte-audited.

The next implementation step is to build the MU0678-specific injected payload using the audited lifecycle while retaining this exact ELF-extension strategy.
