# MU0678 AltScreen runtime experiment

This is the first hardware experiment for the current MU0678 sidecar implementation.

## Payload

The experiment uses exactly three runtime payloads:

- `/eso/bin/apps/dio_manager` — the MU0678 DIO integration with the AltScreen hooks.
- `/eso/lib/libmhi2qcarplaycluster.so` — the MU0678 ARM/QNX cluster sidecar.
- `/eso/bin/apps/mhi2q-aa-cluster-display` — the exact working reference helper from `install.zip`.

The reference helper is copied byte-for-byte from the supplied working package. The MU0678 cluster is the architecture-specific implementation; it does **not** call stock `StartSession(B)`, because MU0678 has a singleton native Screen.

## Host preparation

Build the cluster with:

```sh
tools/build-mu0678-cluster.sh build/libmhi2qcarplaycluster.so
```

Package it with the exact reference helper:

```sh
tools/package-mu0678.sh /path/to/install.zip package-mu0678 build/libmhi2qcarplaycluster.so /path/to/mu0678-dio-manager
```

The resulting directory must contain:

```
package-mu0678/
  eso/bin/apps/dio_manager
  eso/bin/apps/mhi2q-aa-cluster-display
  eso/lib/libmhi2qcarplaycluster.so
```

Copy those three files to a temporary staging directory on the unit; do not overwrite anything yet.

## Install

On the MHI2 as root:

```sh
cd /path/to/staged-payload
DIO_SRC=./dio_manager \
CLUSTER_SRC=./libmhi2qcarplaycluster.so \
HELPER_SRC=./mhi2q-aa-cluster-display \
./runtime-mu0678-experiment.sh install
```

The script creates an exact backup under:

```
/tmp/mu0678-alt111-backup
```

It refuses to overwrite an existing experiment backup. This makes a second install impossible until the first experiment is rolled back.

**Reboot after installation.** Do not hot-restart `dio_manager` for this first run.

## Baseline before connecting iPhone

After reboot:

```sh
./runtime-mu0678-experiment.sh status
./runtime-mu0678-experiment.sh collect
```

Record:

1. normal MMI operation;
2. normal CarPlay operation with no attempt to force a secondary stream;
3. whether the helper process is running;
4. whether TCP/15557 is listening locally.

## Experiment

Connect the iPhone and start normal wired CarPlay.

The expected progression is:

1. primary stream 110 continues to the normal MMI screen;
2. receiver advertisement includes `altScreen`;
3. iPhone negotiates the alternate feature;
4. SETUP reaches the 111 path;
5. the 111 setup creates the B SessionScreen and derives its security material using the same stock AES derivation path;
6. the 111 ScreenStream is created as a sidecar;
7. the secondary ProcessFrames path reaches `ScreenStreamProcessData`;
8. the intercepted encoded access units reach the AAF1 sink;
9. the exact reference helper receives the AAF1 stream;
10. only then should a physical VC/KOMBI picture be considered a success.

The key safety invariant is that the 111 path must not invoke stock `StartSession(B)` on MU0678.

## Immediate capture

As soon as the experiment ends — whether successful, timeout, black screen, or reconnect — run:

```sh
./runtime-mu0678-experiment.sh collect
```

Then save the resulting directory before rebooting again.

Useful additional commands:

```sh
pidin ar
pidin threads
netstat -an
sloginfo
```

## What each outcome means

### A. CarPlay remains normal, no 111

The DIO/`/info` advertisement or iPhone negotiation is still the gate. Preserve the capture; do not change the renderer.

### B. 111 SETUP occurs, but no ProcessData capture

The protocol/session construction is working far enough to reach 111, but the MU0678 stream-processing boundary is not the one exercised by this firmware path.

### C. ProcessData capture occurs, helper receives data, VC remains unchanged

The AirPlay and encoded-data path is working. The remaining issue is the physical DMDT/VC route.

### D. MMI goes black or primary CarPlay times out

Stop the experiment and roll back. This indicates that the sidecar still touched the singleton primary Screen or another shared renderer state.

### E. VC receives a picture while MMI remains normal

That is the target simultaneous-output result. Preserve the exact logs and payload hashes before changing anything.

## Rollback

On the unit:

```sh
./runtime-mu0678-experiment.sh rollback
```

Then reboot.

Rollback restores the exact files that existed before the experiment; it does not reconstruct them from the repository.

## Runtime evidence to preserve

For every run, preserve:

- timestamp;
- payload SHA-256 values;
- `sloginfo`;
- `pidin ar`;
- `pidin threads`;
- `netstat -an`;
- whether 110 remained visible;
- whether 111 SETUP occurred;
- whether `ScreenStreamProcessData` was reached;
- whether helper 15557 traffic occurred;
- whether VC/KOMBI changed;
- whether the session survived reconnect.

No physical display ID should be declared proven from this experiment unless the corresponding DMDT/physical output evidence is captured.
