# Empty Pack Fixture

`fixtures/packs/empty-v0.mpack` is the first structural regression fixture.

It is intentionally:

- minimal;
- non-executable;
- section-free;
- marked as a test fixture;
- deterministic;
- not a promise that the M2 production pack format cannot evolve.

## M0 binary layout

All integer fields are little-endian.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | magic `MOSPACK\0` |
| 8 | 2 | format major = 0 |
| 10 | 2 | format minor = 1 |
| 12 | 2 | header length = 32 |
| 14 | 2 | flags; bit 0 = test fixture |
| 16 | 8 | file length = 32 |
| 24 | 4 | section count = 0 |
| 28 | 4 | reserved = 0 |

M2 will introduce canonical content hashing, dependency sections, typed section directories, and executable declarative data. The M0 fixture's purpose is to make loader behavior testable before those features exist.
