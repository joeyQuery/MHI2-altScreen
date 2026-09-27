# Authoritative working-package audit — 2026-09-27

## Reference

The authoritative working implementation for this project is the supplied `install.zip`, release:

```
MHI2Q_ER_AUG22_P5092_MU1329
MIBSI_RELEASE_ID=7e792409480209467f17fb27
```

This audit supersedes comparisons against experimental `libairplay_*` artifacts.

## Critical finding: libairplay is STOCK

The package installer explicitly verifies the target AirPlay library before installation:

```
AIRPLAY_SHA="01c116b1674215daa0602d556a6d9b2eb48c2113ca294ea1f5d58787e3f656f2"

mibsi-install-tool verify "$AIRPLAY_SHA" /eso/lib/libairplay.so
```

Therefore the working package requires the exact expected `libairplay.so` and does **not** replace or patch it.

This is the authoritative correction to the earlier experimental-ELF comparison.

## Working package binaries

| Component | Size | SHA-256 |
|---|---:|---|
| `libmhi2qcarplaycluster.so` | 63,375 | `e3fa80fb3326bd470df6a1dc2c6866148b67e8f5d19bb1d98c97f4662f6d76c9` |
| `libmhi2qaacluster.so` | 61,055 | `171bc1e873f84a8877ce4ed261054476e67d4cb9ba851c62efbb1f5b8bec84a1` |
| `dio_manager.mhi2q_carplay_loader` | 695,420 | `4dbbb057fe5ccbd95fbea71b16810cbdd417ca60a7aa6113e7918ac97e252e94` |
| `gal.mhi2q_aa_cluster_loader` | 1,405,891 | `35586d98a70909cdd1cae18a876a6b7b0141866080c0c903a4182d51c1fda254` |
| `mhi2q-aa-cluster-display` | 31,899 | `0ad556112e427745da99e057affde5b18195db875398d211cb131f790b7d65e` |
| `dio_manager.json` | 6,473 | `234e19e9360f2e46f1487a6cf800b2bc07d2a5d3489f8a9f89f2219a833bfc20` |

## CarPlay cluster dynamic surface

`libmhi2qcarplaycluster.so` contains runtime-resolved references to:

- `AirPlayReceiverSessionSetup`
- `AirPlayReceiverSessionTearDown`
- `AirPlayReceiverSessionSendCommand`
- `AirPlayReceiverSessionForceKeyFrame`
- `AirPlayReceiverSessionScreen_Create`
- `AirPlayReceiverSessionScreen_Delete`
- `AirPlayReceiverSessionScreen_Setup`
- `AirPlayReceiverSessionScreen_SetTimeSynchronizer`
- `AirPlayReceiverSessionScreen_SetSecurityInfo`
- `AirPlayReceiverSessionScreen_UpdateState`
- `AirPlayReceiverSessionScreen_StartSession`
- `AirPlayReceiverSessionScreen_ProcessFrames`
- `AirPlayReceiverSessionScreen_StopSession`
- `ScreenStreamCreate`
- `ServerSocketOpen`

Its read-only data also contains the exact protocol/property strings:

- `altScreen`
- `streamConnectionID`
- `dataPort`
- `features`
- `displays`
- `maxFPS`
- `widthPhysical`
- `heightPhysical`

This establishes that the Q implementation is an external native module operating against the stock AirPlay ABI, rather than a modified `libairplay.so`.

## DIO loader surface

`dio_manager.mhi2q_carplay_loader` resolves:

- `AirPlayReceiverSessionChangeModes`
- `AirPlayReceiverSessionScreenSetDelegate`
- `AirPlayReceiverSessionTearDown`
- `ScreenCreate`
- `ScreenRegisterDelegates`
- `ScreenRegister`
- `_ScreenSetProperty`

This is the native DIO/Screen integration side of the working solution.

## Consequence for the MHI2 replica

The correct replication target is:

```
stock MU0678 libairplay.so
        |
        +-- existing 110 CarPlay path
        |
        +-- external secondary-screen implementation
              |
              +-- Q-equivalent SessionScreen B
              +-- Q-equivalent 111 negotiation/advertisement
              +-- second listener
              +-- secondary socket/ProcessFrames
              +-- native Screen/DIO association
              +-- decoder/output integration
```

The previous audit that treated an 8-byte `libairplay.so` modification as the authoritative working patch is withdrawn.

## Installer-level proof

The package also records the exact hashes for all managed binaries in `install.sha256` and repeats the AirPlay dependency check in `install_product.sh`. The package therefore provides cryptographic identity for the actual working implementation and its required stock dependencies.

## Current boundary

This document establishes the authoritative package identity and, critically, that the working implementation does not patch `libairplay.so`.

It does **not** by itself claim that every imported symbol is functionally exercised on every run. Function-level call order remains governed by the binary traces in the rest of this repository.
