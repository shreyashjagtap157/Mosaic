# Reference Hardware Policy

Status: hardware assignment pending.

Mosaic does not currently have a declared physical reference machine in this repository. Performance claims are therefore forbidden until one or more real machines are assigned and recorded.

## Required benchmark-machine record

Every published benchmark must record:

- manufacturer/model;
- CPU model, microcode if relevant, sockets, physical/logical cores;
- ISA extensions enabled;
- memory capacity, channels/speed where known;
- storage device/model for I/O-sensitive runs;
- OS/kernel/build;
- power/performance governor;
- virtualization/container status;
- compiler and linker versions;
- Rust toolchain;
- build flags/LTO/codegen units;
- thread affinity policy;
- temperature/throttling notes for long campaigns.

## Recommended reference profiles

Rather than one mythical universal machine, maintain at least:

1. x86-64 mainstream AVX2 workstation/server profile;
2. ARM64 NEON profile;
3. scalar/feature-disabled correctness profile;
4. optional AVX-512 server profile;
5. later constrained/embedded profile.

Results across profiles are separate data, not averaged into one meaningless "Mosaic speed" number.
