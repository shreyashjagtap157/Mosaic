# Mosaic Tokenizer 0.7.0

0.7 adds the first production Unicode-security evidence layer while preserving the exact-byte tokenizer contract.

Highlights:

- deterministic `security17-v1.mpack` generated from a Unicode-17 property oracle;
- 176 Script values with exact source-byte script spans;
- bidi-control, default-ignorable, noncharacter, deprecated-character findings;
- deterministic mixed-script evidence;
- standalone and high-level tokenizer security APIs;
- CLI `security`, `fingerprint-security`, and `analyze-security`;
- security pack included in release bundles and fingerprints;
- 11 malformed security pack classes under sanitizer coverage;
- C API surface 0.5.0.

This release does **not** claim confusable-skeleton detection yet. UTS #39-style confusable mappings require a separately pinned confusables data pack and will not be approximated from script heuristics.
