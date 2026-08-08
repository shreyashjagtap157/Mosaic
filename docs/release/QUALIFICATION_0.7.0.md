# Qualification — Mosaic Tokenizer 0.7.0

0.7 inherits all 0.6 tokenizer, authoring, compatibility, language, detector, Unicode segmentation, streaming-reference, edit-reference, sanitizer, and package gates.

## Unicode/security gates

- deterministic Unicode-17 security-pack reconstruction;
- all 176 Script values present with stable IDs;
- independent `regex` Unicode-17 oracle comparison over 311 random/crafted property cases;
- exact invalid-UTF-8 script-0 preservation;
- bidi-control/default-ignorable/noncharacter/deprecated finding comparison;
- deterministic mixed-script primary/non-primary selection;
- 11 structurally rehashed malformed security packs rejected fail-closed;
- ASan/UBSan security API test;
- high-level tokenizer attachment, duplicate rejection and fingerprint change;
- Clang static analyzer clean after explicit output-capacity guards;
- security pack clean-extraction CLI and external C consumer tests.

## Security memory hardening

The original release-candidate security implementation materialized one `ScalarUnit` per possible input byte and did so separately for script and finding scans. On the deliberately mixed 10 MiB fixture this drove peak RSS to roughly 196 MiB. That implementation was rejected before release.

The accepted implementation uses strict UTF-8 streaming passes and exact output sizing. It no longer allocates a decoded-scalar array for security scans. Array-returning APIs still allocate the script/finding arrays explicitly requested by the caller. Therefore memory may remain proportional to output density. Measured through the standalone CLI, which requests both arrays:

- 10 MiB Latin fixture with frequent Latin/Common transitions: **~85.4 MiB RSS**, **1.84 s**;
- 10 MiB deliberately mixed/security-heavy fixture: **~133.9 MiB RSS**, **1.36 s**.

A callback/visitor evidence API remains planned for the bounded-processing phase so callers can avoid materializing large result arrays. This is an API-output limitation, not hidden tokenizer state.

## Base tokenizer regression

10 MiB mixed English/Hindi/CJK source, one warm-up plus three measured runs:

- elapsed: `0.17 / 0.17 / 0.17 s`;
- median throughput: **58.8 MiB/s**;
- peak RSS: **51.6 MiB**;
- native CLI size: **79,816 bytes**.

## Reproducibility identities

- base tokenizer fingerprint: `b2676d5b09dcafe993880590d63f20c9bf1f777a22ef6fde952c8d4dcc4348a5`;
- language fingerprint: `8732f45005620b85293f418d70d708ba93694d97cec2867dd5c41cc364e44b31`;
- detector/language fingerprint: `ae09203a99f9014ac933401e97db9fefcd4e1ef9919402c6c7c02bb098704e32`;
- security-enabled fingerprint: `023bf2e40d9ad7943c0bd727c5f9a739b92c3773598e1d08221c8537b46f726e`;
- release archive SHA-256: `bbb23d0d009c960ce671d3aeb33aef0cc7237044f09e67fd6a2a6006c1a363da`.

## Qualification limitation

The stable Rust reference remains uncompiled on this host because `rustc`/`cargo` are unavailable and outbound package/network access is disabled. The native C runtime is qualified with GCC, Clang, ASan/UBSan, malformed-pack tests, an independent Python Unicode/property oracle, and clean-extraction package tests.
