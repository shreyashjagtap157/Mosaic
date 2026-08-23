# Mosaic Enterprise Threat Model

Status: frozen enterprise candidate baseline; current product release 0.1.3.4.

## Security objectives

Mosaic must preserve memory safety of the native process, exact source identity, canonical deterministic interpretation, bounded resource use under configured policies, and provenance/trust boundaries when processing attacker-controlled source bytes, packs, serialized TokenDocuments, cache records, registry state, FFI calls and observer callbacks.

Mosaic does **not** claim vulnerability freedom, safe execution of arbitrary native plugins, semantic truth of language detection, or confidentiality of source bytes supplied to an application that itself exposes them.

## Trust boundaries and assets

### Untrusted source bytes

Threats: malformed UTF-8, adversarial Viterbi ambiguity, huge inputs, pathological incremental edits, control-looking text, binary data.

Controls: byte-authoritative representation, complete byte fallback, checked arithmetic, configurable source/output ceilings, bounded online state, exact differential tests, no plain-text privileged-control activation.

### Untrusted packs

Threats: corrupt offsets/lengths, overlapping sections, bogus state transitions, dependency ambiguity, resource bombs, malicious metadata, signature laundering.

Controls: structural validation before execution/trust, canonical padding and hashes, hard section/resource ceilings, no native code in ordinary packs, content-addressed identity, malformed-pack corpus, Ed25519 trust only after structural validation.

### Serialized TokenDocument / packed model / cache records

Threats: forged counts, integer overflow, section aliasing, source/fingerprint mismatch, hidden trailing data, allocation bombs, tampering/replay.

Controls: endian-defined canonical records, whole-record authentication hashes, source/tokenizer identities, structural validation after integrity verification, caller-configurable deserialization ceilings, hostile re-authenticated record tests.

### Cache backends

Threats: stale/missing/corrupt/replayed values, backend availability failure, oversized records, wrong-key substitution.

Controls: content-addressed keys, authenticated cache-record protocol binding key/value/length, max-record ceilings, fail-closed validation before promoting to hot cache. Cache correctness never changes canonical tokenizer semantics.

### Registry/database/object store

Threats: SQLite corruption, object corruption/deletion, mutable resolution, concurrent install conflicts, attacker-controlled aliases, rollback.

Controls: WAL transactions, exact object hashes, deterministic SemVer resolution to exact lock graph, no mutable `latest` in canonical execution, database integrity/schema audit, lock verification, exact-hash repair, GC only for unreferenced objects.

### FFI callers

Threats: NULL/invalid argument combinations, ownership confusion, double-free, stale handles, post-seal mutation, ABI mismatch.

Controls: opaque ownership-bearing handles, Mosaic-owned buffers freed only with `mosaic_free`, explicit status codes, argument validation, sealed runtime state, C/C++/Python external-client tests, frozen ABI contract. The C ABI cannot make an arbitrary invalid pointer safe to dereference; callers remain responsible for passing memory valid for declared lengths.

### Concurrency

Threats: data races, use-after-free across worker threads, callback reentrancy, queue exhaustion, unbounded work submission.

Controls: immutable sealed tokenizers for shared executor use, bounded worker/queue/batch/input budgets, private Win32/pthread portability layer, cache mutexing, thread-safe atomic metrics, observer reentrancy guard, deterministic result ordering, concurrent sanitizer/reliability tests. ThreadSanitizer remains a platform/toolchain qualification gate where available.

### Observability callbacks

Threats: source leakage, recursive callback storms, callback races, application callback faults.

Controls: metadata-only events with no source/token surfaces, observer configured before seal, same-tokenizer recursion suppression, documented callback thread-safety requirement. Mosaic cannot recover from arbitrary memory corruption inside a user callback executing in-process.

### Build and supply chain

Threats: dirty or unreproducible release, altered binaries/packs, dependency confusion, provenance fiction, missing SBOM.

Controls: clean-tree official release rule, deterministic archive and wheel builds, SHA-256 inventory, SPDX SBOM, provenance binding Git revision/source-manifest hash, content-addressed pack registry, optional signed packs, mechanically synchronized version metadata.

## Primary abuse cases

- memory/resource denial of service via huge or combinatorial input;
- crafted pack/record causing out-of-bounds access or arithmetic overflow;
- substitution of a valid pack for a different expected pack;
- malicious cache/backend returning data under the wrong key;
- registry object corruption followed by unnoticed execution;
- observer/FFI lifecycle misuse under concurrency;
- Unicode security evidence confused with source rewriting;
- language detector confidence treated as correctness;
- performance optimization diverging from canonical reference semantics.

## Non-negotiable invariants

- original source bytes are never rewritten as canonical truth;
- unknown source-byte rate is zero;
- canonical execution depends only on exact resolved identities and declared configuration;
- optimized/parallel/incremental/streaming paths equal reference canonical semantics;
- ordinary packs execute no native code;
- failure to authenticate/validate/fit resource policy fails closed and never silently falls back to a different canonical segmentation.

## Residual risks before/after stable-generation graduation

- platform-specific compiler/runtime defects;
- third-party OpenSSL/ICU/Python vulnerabilities when optional features use them;
- application misuse of the C ABI or observer callback;
- corpus bias and detector/language-pack quality errors;
- denial of service within deliberately high operator-configured ceilings;
- undiscovered algorithmic worst cases;
- Rust implementation remains independently unqualified in the current sandbox until Rust CI runs.

These risks are managed through CI, fuzzing, coordinated disclosure, dependency updates, policy ceilings and explicit deployment guidance rather than hidden by a claim of absolute security.
