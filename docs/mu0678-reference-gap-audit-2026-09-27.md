# MU0678 reference gap audit — 2026-09-27

## Authoritative reference

This audit uses the supplied `install.zip` as the functional reference.

Reference hashes verified from the package:

- `dio_manager.mhi2q_carplay_loader`: `4dbbb057fe5ccbd95fbea71b16810cbdd417ca60a7aa6113e7918ac97e252e94`
- `libmhi2qcarplaycluster.so`: `e3fa80fb3326bd470df6a1dc2c6866148b67e8f5d19bb1d98c97f4662f6d76c9`
- `mhi2q-aa-cluster-display`: `0ad556112e427745da99e057affde5b18195db875398d2111cb131f790b7d65e`

The reference package's product installer explicitly verifies the stock `libairplay.so` SHA rather than replacing it. The external cluster is therefore the correct implementation boundary.

## Concrete defects found in the current replica

### 1. SETUP veneer stack ABI was wrong

The MU0678 SETUP hook is installed at `libairplay + 0x23260`.

At that boundary the proven state is:

- `r6` = session
- `r9` = current stream
- `r10` = AirPlay key base
- original `[sp+0x68]` = setup result

The old veneer pushed 14 registers (56 bytes) but then loaded the setup result from `[sp+0x88]` and another argument from `[sp+0x9c]`. Those slots were not the boundary arguments.

The corrected veneer uses:

- `[new_sp+0xa0]` for the original setup result
- `r9` for the current stream
- `r10` for the key base

This now matches the proven boundary ABI.

### 2. NvSS decode veneer literal addresses were wrong

The raw decode call is `libairplay + 0x7fb68), with continuation `+0x7fb6c`.

The previous veneer used literal addresses that did not correspond to the ARM PC-relative `ldr` offsets. The corrected veneer lays out the three literals at the exact addresses its instructions reference and branches to the veneer decode entry at `+0x34`.

The resulting sequence is:

1. read original length from `[sp+0x14]`;
2. save original registers;
3. call the secondary-only tee;
4. restore original registers and `lr`;
5. load the original continuation into `lr`;
6. branch through the original NvSS decode PLT.

### 3. AirPlay symbol resolution did not match the reference architecture

The reference cluster contains the `libairplay.so` string and dynamically resolves AirPlay symbols from that library.

The replica previously used `dlsym(NULL,...)`.

The replica now opens `libairplay.so` using the same observed `0x80000100` loader mode and resolves all AirPlay/CF helpers through that handle.

### 4. Response dictionary allocator was fabricated

The old replica read a byte from `session+0x1408` and passed it as the CF allocator/capacity input to `CFDictionaryCreateMutable`.

That field was not proven.

The reference construction uses a null/default allocator semantics and zero initial capacity with the recovered key/value callback tables. The replica now calls:

```
CFDictionaryCreateMutable(0, 0, keys, values)
```

using the proven MU0678 callback tables at:

- key callbacks: `libairplay + 0xaa3b8`
- value callbacks: `libairplay + 0xaa390`

and keys:

- type: `libairplay + 0x941dc`
- data port: `libairplay + 0x942c4`

### 5. Hook installation was not transactional

A failed second code-hook installation could previously leave the first code patch and ScreenStreamCreate GOT hook active.

The installer now records the original GOT/SETUP state and rolls back the earlier hook when a later hook fails.

### 6. The external cluster had no load-time entry point

The reference architecture loads `libmhi2qcarplaycluster.so` into the DIO process. The current source now invokes `mu0678_interpose_init()` from a shared-library constructor so loading the cluster actually installs the guarded MU0678 interposition.

This does not claim that the hardware DIO callback/session-finalization boundary is solved; that remains target/runtime validation.

### 7. Build/package identity differed from the reference

The build script previously emitted `libmhi2mu0678altscreen.so`.

The working loader searches for `libmhi2qcarplaycluster.so`. The build now emits that identity and sets the same SONAME.

The package script now stages:

- the exact reference `dio_manager.mhi2q_carplay_loader` as the DIO loader;
- the newly built `libmhi2qcarplaycluster.so`;
- the exact reference `mhi2q-aa-cluster-display`.

## Deliberately unchanged translations

These are target-specific MU0678 translations rather than copied Q absolute values:

- secondary listener owner byte remains the MU0678 stock caller's `session+0x51`;
- `ServerSocketOpen(owner,1,6,0xffffe872,&port,0xfffd4000,&fd)`;
- AES material remains `session+0x1c8`, length 16, negotiated 64-bit value;
- MU0678 ProcessFrames context remains `session+0x0c` then `context+0xc8`;
- secondary decoder capture remains `ScreenStream+0x48`;
- raw NvSS tee remains at `0x7fb68`;
- stock 110 is preserved.

The Q `session+0x55` listener owner seen in the reference package is not copied into MU0678 because the MU0678 native listener caller was independently recovered at `0x23380` with `session+0x51`.

## Remaining evidence boundary

Static source/package work is materially closer to the supplied reference, but the following still require the actual MU0678 runtime:

1. prove the replacement DIO loader loads the newly built cluster on the target;
2. prove the 111 SETUP hook is reached without disturbing 110;
3. prove the advertised secondary listener receives the iPhone 111 connection;
4. prove SessionScreen B starts and decrypts the secondary stream;
5. prove the captured decoder receives only 111 traffic;
6. prove the exact helper accepts the MU-produced AAF1 stream;
7. prove the MU-native DMDT/Screen association renders on the intended VC surface;
8. prove teardown/reconnect under repeated sessions.

No claim of complete hardware success is made here.
