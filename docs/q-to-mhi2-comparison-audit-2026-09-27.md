# Q-to-MHI2 comparison audit — 2026-09-27

## Purpose

This is a consistency audit between the supplied authoritative working Q package and the current MHI2-AltScreen research/specification. The Q package remains the reference. This audit distinguishes Q behaviour proven in the binary, MU0678 behaviour proven in the MHI2 dump, functional translations intentionally chosen, and implementation work that is not yet present in this repository.

## Result

The functional architecture is correct. The important Q operations have been reproduced as the correct MHI2-side requirements, without copying Q-only offsets or constants.

The audit found one bookkeeping error in repository wording: several STATUS statements said the "current MHI2 implementation already contains" the secondary path. The MHI2-AltScreen repository is currently the reverse-engineering/specification repository; it does not itself contain a completed MU0678 binary implementation. Those statements have therefore been corrected to describe the already-proven/traced MHI2 replica work rather than claim a finished patch.

## Operation-by-operation comparison

| Q operation | Authoritative Q evidence | MHI2 translation | Audit |
|---|---|---|---|
| Preserve 110 | Separate 110/111 handling | Leave stock 110 path unchanged | CORRECT |
| Detect 111 | SETUP scans stream collection and compares type 111 | Add/retain 111 branch in MU0678 SETUP | CORRECT |
| Create B | Second SessionScreen | MU0678-native SessionScreen_Create | CORRECT |
| UpdateState(B,1) | Q type-111 path | Same operation on B | CORRECT |
| Setup(B,111,CreateResult) | Q passes actual CreateResult as arg 3 | MU0678 ScreenSetup overwrites caller r2 with its own output storage | CORRECT TRANSLATION; do not copy Q ABI assumption |
| Time synchronizer | Q sets it before security | MU0678 SetTimeSynchronizer copies 12 bytes to screen+0x20 | CORRECT |
| Negotiated 64-bit value | Q reads the 111 negotiated value | MU0678-native stream field/representation | CORRECT, but field must remain MU0678-specific |
| AES derivation | Q uses session+0x1c0, length 16, negotiated 64-bit value | Same primitive with MU0678-native inputs | CORRECT |
| SetSecurityInfo(B) | Q consumes two derived 16-byte buffers | Same MU0678 export | CORRECT |
| Secondary listener | Q opens second listener | MU0678 7-arg ServerSocketOpen using stock screen/media caller values | CORRECT |
| Advertise 111/dataPort | Q inserts type 111 + dataPort | Same response structure | CORRECT |
| Accept | Q accepts after advertisement | MU0678 SocketAccept | CORRECT |
| NetSocket wrapper | Q wraps accepted fd | MU0678 NetSocket_CreateWithNative | CORRECT |
| StartSession(B) | Q invokes it only in post-accept worker | **Not called on MU0678** because stock StartSession binds the singleton main Screen; B is translated to a sidecar ScreenStream/process-data path | ARCHITECTURE-SPECIFIC EXCEPTION |
| ProcessFrames(B,socket) | Q calls after accepted socket + StartSession | MU0678 ProcessFrames(B, socket) | CORRECT |
| Decoder | Q OMX lifecycle | MU0678 NvSS | CORRECT platform translation |
| ScreenStream +0x38 | Q captures it but does not consume captured value later | Not required for parity | CORRECT |
| Native ScreenCreate | Q has one direct loader ScreenCreate call, not a proven second 111-specific factory | Do not invent a second native ScreenCreate | CORRECT |
| Q output helper | Q has real mhi2q-aa-cluster-display | MU0678 can use analogous dedicated output helper | CORRECT functional translation |
| Loopback listener | Q uses 15557 | MU0678 should use a private local transport; Q's port is not transplanted | CORRECT |
| Loopback writer | Q accepted fd is stored and later passed to write() | Dedicated MHI2 secondary sink must own its fd | CORRECT |
| Display geometry | Q helper uses 1440x540 | MU0678 recovered KOMBI displayable is 1440x455 | CORRECT; Q geometry must not be copied |
| DMDT route | Q helper invokes context 74 | MU0678 should use its native DMDT/KOMBI routing | CORRECT, but physical panel identity remains runtime-specific |
| ScreenCopyMain | Q evidence does not justify globally changing it | Leave MU0678 stock path untouched | CORRECT |
| Teardown | Q performs secondary cleanup alongside original session | Implement equivalent independent cleanup | CORRECT requirement; exact MU0678 ordering still needs runtime verification |

## Exact Q patch mechanics versus functional replication

The Q patch map contains these implementation wrappers:

- SETUP: 0x3C60
- TearDown: 0x33F0
- /info: 0x48C4
- ScreenStreamCreate: 0x4710
- ScreenSetup: 0x46D4
- OMX lifecycle: 0x1954, 0x19B8, 0x1C04, 0x1B8C, 0x19F4

These are Q implementation mechanics. They are not a list of addresses that should be transplanted into MU0678.

The correct replication we established is operation-level:

Q operation -> MU0678-native equivalent at the corresponding AirPlay/Screen/Socket/NvSS ABI.

This is the correct interpretation of the reference patch.

## Security comparison

Q:

DeriveAESKeySHA512ForScreen(session+0x1c0, 16, negotiated64) -> keyA/keyB -> SetSecurityInfo(B)

MU0678:

- the same derivation primitive exists;
- the MU0678 SETUP path obtains the negotiated 64-bit value from its own stream object;
- the derived buffers feed SetSecurityInfo.

No new 111-specific security ABI was invented. This is a direct functional replication of the stock AirPlay security flow.

## Socket comparison

Q uses:

ServerSocketOpen(session+0x55, 1, 6, 0x17D4, ..., -1, ...)

MU0678 stock screen/media listener uses:

ServerSocketOpen(*(uint8_t *)(session+0x51), 1, 6, 0xFFFFE872, ..., 0xFFFD4000, ...)

Therefore the Q constants were correctly not copied.

The RAOP event listener at MU0678 0x23C48 was correctly rejected as the translation target because it is the 5001 event channel.

## Output-boundary comparison

The strongest Q evidence is now:

accepted 111 socket -> H.264/queue state -> loopback writer -> 127.0.0.1:15557 -> mhi2q-aa-cluster-display --live-carplay -> QNX Screen

The helper explicitly selects IVRDISPLAY=SCREEN, dynamically resolves the QNX Screen/display stack, and invokes its DMDT route.

The MU0678 equivalent is therefore not "change the main Screen selector". It is:

B/NvSS -> independent H.264 output -> dedicated QNX Screen sink -> MU0678 DMDT/KOMBI route

This preserves 110.

## Corrections required

### Correction 1 — repository implementation status

Replace wording such as:

"Current MHI2 implementation already contains Type 111 detection, second SessionScreen, ..."

with:

"Current MHI2 reverse-engineering work has statically established the Type 111 detection, second SessionScreen lifecycle, security, listener, socket, ProcessFrames, and output-boundary requirements."

The actual MU0678 patch/loader implementation is not contained in this documentation repository yet.

### Correction 2 — MHI2 output closure wording

The output boundary is architecturally closed, not runtime-proven.

The exact implementation architecture is established. The actual MU0678 helper process, DMDT activation, and simultaneous 110+111 execution still require implementation and hardware validation.

### Correction 3 — physical VC wording

DMDT context 74 is proven as a Q helper request. The MU0678 KOMBI displayable is proven as the target logical display resource. Neither fact alone proves the physical panel mapping without runtime/hardware evidence.

## Final audit verdict

The Q reference has been replicated correctly at the functional level.

The following were copied/reproduced correctly:

- 111 detection;
- independent SessionScreen B;
- UpdateState;
- Setup/security sequence;
- AES derivation structure;
- secondary listener;
- 111/dataPort response;
- accept + NetSocket;
- post-accept worker ordering, with MU0678-specific omission of stock StartSession to preserve the singleton main Screen;
- ProcessFrames;
- independent decoder/output path;
- preservation of 110.

The following were correctly not copied:

- Q absolute addresses;
- Q ScreenStream object offsets;
- Q socket constants;
- Q +0x38 diagnostic capture as a functional requirement;
- Q OMX object layout;
- Q 1440x540 geometry;
- Q's native ScreenCreate call as proof of a second 111 screen;
- global ScreenCopyMain modification.

The remaining gap is implementation/runtime, not a mismatch in the recovered Q functional model.
