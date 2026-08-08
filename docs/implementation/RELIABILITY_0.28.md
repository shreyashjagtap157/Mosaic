# Mosaic 0.28 Reliability and Recovery Qualification

Status: implementation baseline for the 0.28 enterprise reliability release.

## Goals

0.28 adds almost no tokenizer semantics. It makes existing semantics survive sustained, hostile and recoverable operation. The campaign is deterministic by seed so a failure is replayable rather than merely anecdotal.

## Native campaign

`conformance/c/reliability_smoke.c` continuously verifies:

- arbitrary-byte encode/decode identity;
- online streaming equals one-shot tokenization under randomized chunking;
- incremental edits equal fresh full tokenization and preserve exact edited bytes;
- cold TokenDocument serialize/deserialize/reserialize is byte-identical;
- repeated pack load/fingerprint/free lifecycle;
- bounded multi-worker executor batches preserve order and exact decode;
- runtime metrics report no hidden failures.

The campaign emits a deterministic 64-bit replay digest over semantic results. A repeated run with the same seed must produce the same textual summary and digest.

## Control-plane chaos

`tools/reliability_campaign.py` additionally verifies the local registry under operational faults:

1. concurrent idempotent installs converge on one content-addressed object;
2. exact lock resolution and audit are initially clean;
3. an object is deliberately corrupted;
4. both lock verification and registry audit must fail;
5. `mosaic-registry repair` restores only exact bytes whose hash is already referenced by registry metadata;
6. the same lock and audit become valid again;
7. garbage collection removes only an unreferenced object.

SQLite `PRAGMA integrity_check` and schema metadata are part of audit.

## Tiers

| Tier | Native iterations | Intended use |
|---|---:|---|
| smoke | 2,000 | PR/local fast reliability |
| release | 25,000 | nightly and release qualification |
| soak | 250,000 | manual/extended enterprise soak |

Maximum per-input bytes remain bounded by the native harness. The campaign is workload-count based rather than wall-clock based so results are reproducible across machines.

## Initial release evidence

The first 25,000-iteration release campaign on the Linux qualification host passed twice with identical replay digest:

```text
replay=c245b66401c94ed2
iterations=25000
online=1087
incremental=353
cold=191
lifecycle=50
batches=50
encode_calls=31944
```

Registry chaos also passed with 16 concurrent installs, deliberate corruption detection, exact repair, lock re-verification and one orphan garbage-collected.

## Non-claims

This is not proof of failure-free operation for all hardware or operating systems. Windows/macOS execution, ThreadSanitizer where supported, substantially longer soak campaigns and production workload distributions remain release/enterprise deployment gates. The campaign is designed so those future runs use the same deterministic workload contract.
