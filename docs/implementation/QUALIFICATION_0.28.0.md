# Mosaic 0.28.0 Qualification

Date: 2026-08-08

- inherited strict GCC build and native suite: PASS
- inherited ASan/UBSan suite: PASS
- reliability ASan/UBSan smoke: 500 iterations PASS
- normal reliability gate: 1,000 iterations PASS
- independent Clang CMake/CTest: 24/24 PASS
- release campaign repeated deterministically: 25,000 iterations x2 PASS
- release replay digest: `c245b66401c94ed2`
- soak campaign repeated deterministically: 250,000 iterations x2 PASS
- soak replay digest: `8bec191c6e5dd904`
- registry concurrent idempotent installs: 16 PASS
- deliberate object corruption detected by lock verification and audit: PASS
- exact-hash atomic registry repair: PASS
- lock/audit recovery after repair: PASS
- unreferenced object GC isolation: PASS

The reliability workload is deterministic by seed and operation count. It is not a wall-clock-only soak and therefore produces replayable semantic evidence.
