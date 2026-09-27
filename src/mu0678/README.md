# MU0678 external 111 cluster

This directory is the implementation translation of the supplied MHI2Q reference.

The design deliberately preserves stock MU0678 libairplay.so.

Components:
- mu0678_cluster.c — MU0678-native secondary SessionScreen lifecycle and AAF1 output sink.
- mu0678_cluster.h — narrow ABI boundary.
- build-mu0678-cluster.sh — ARM EABI5 shared-object build using LLVM/lld.

Fixed translations:
- Q session+0x0c -> MU0678 session+0x0c
- Q ProcessFrames context context+0x1c8 -> MU0678 stock context+0xc8
- Q OmxVideoImpl -> MU0678 NvssVideoImpl
- Q live output -> MU0678-compatible AAF1 writer
- Q 1440x540 -> MU0678 logical DISPLAYABLE_KOMBI_MAP_VIEW 1440x455

Deliberately not hard-coded: the AirPlay SETUP trampoline/callback patch sites and physical DMDT route are target-specific and require the exact MU0678 DIO image. This source exposes explicit entry points rather than silently guessing those ABIs.
