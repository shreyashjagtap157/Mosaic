# Mosaic Tokenizer 0.6.0

0.6 adds the first exact existing-tokenizer compatibility execution profile.

## Added

- raw-byte BPE model algorithm alongside weighted Viterbi;
- byte fallback is now surface-based rather than requiring token ID == byte value;
- `.tiktoken` mergeable-rank import and exact eligible export;
- independent 608-case arbitrary-byte raw-BPE differential oracle;
- native public-API and ASan/UBSan BPE smoke coverage;
- invalid algorithm/duplicate-rank fail-closed tests;
- compatibility fixture with deliberately permuted single-byte token IDs.

## Compatibility boundary

0.6 is exact for raw/single-piece BPE with identical mergeable ranks. It does not claim ordinary tiktoken text encoding compatibility because tiktoken additionally applies an encoding-specific Unicode regex pre-tokenizer and special-token protocol.
