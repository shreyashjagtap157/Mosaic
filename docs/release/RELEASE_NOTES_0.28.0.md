# Mosaic Tokenizer 0.28.0

Mosaic 0.28.0 is the enterprise reliability and recovery qualification release. It intentionally adds almost no tokenizer semantics. Instead it makes the existing native runtime and registry control plane survive deterministic replay, sustained arbitrary-byte processing, online/incremental/cold-document differentials, concurrent execution, object corruption, exact repair, and garbage collection.

## Reliability contract

- deterministic seed-driven native campaign;
- one-shot arbitrary-byte encode/decode identity;
- randomized online-streaming equivalence;
- transactional incremental-edit equivalence;
- cold TokenDocument byte-identical reserialization;
- repeated pack lifecycle churn;
- bounded parallel executor batches;
- registry concurrent install/corruption/repair/GC campaign;
- PR smoke, nightly/release, and explicit soak tiers.

## Release evidence

The 25,000-iteration release tier ran twice with identical replay digest `c245b66401c94ed2`.

The 250,000-iteration soak tier ran twice with identical replay digest `8bec191c6e5dd904` and included:

- 10,870 online-stream differentials;
- 3,522 incremental-edit differentials;
- 1,909 cold TokenDocument round trips;
- 498 pack lifecycle reloads;
- 200 bounded executor batches;
- 281,031 tokenizer encode calls reported by runtime metrics.

No tokenizer semantic, public ABI layout, pack-format, or TokenDocument-format change is introduced by this release.
