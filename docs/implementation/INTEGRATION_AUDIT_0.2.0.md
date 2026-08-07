# Mosaic Integration Audit — 0.2.0

## Result

**The implemented tokenizer components form one coherent system for the declared 0.2 scope.**

The runtime is no longer a collection of independent experiments. `mosaic_tokenizer` owns the validated model and Unicode packs plus an optional set of validated language packs. All high-level encode, token-span, stream, document, Unicode, and fingerprint operations are reachable from that single facade.

## Integration path

```text
exact source bytes
  -> validated model pack (mandatory 256-byte fallback)
  -> optional external language-pack projection onto model costs
  -> deterministic integer Viterbi selection
  -> token IDs + byte spans

exact source bytes
  -> validated Unicode 17 pack
  -> grapheme view mapped to the same byte coordinates
```

Streams and editable documents created from an integrated tokenizer snapshot the effective language-adjustment vector; they do not depend on later parent mutation.

## Direction audit

The implementation remains aligned with the architecture:

- bytes remain authoritative;
- language packs are optional and cannot affect representability;
- ordinary packs are declarative data;
- model IDs remain model-pack-defined;
- pack effects are deterministic and fingerprinted;
- expensive composition work moved from the hot path to pack attachment;
- malformed packs fail before becoming live tokenizer state;
- low-level model APIs remain pack-neutral compatibility paths.

## Performance correction made during implementation

The first language composition implementation searched every loaded language pack for every candidate token. On a 10 MiB mixed fixture it produced the intended ~27% token reduction but took about 0.80 s versus 0.21 s unspecialized.

It was rejected. The implementation now preprojects pack adjustments onto vocabulary entries once at attachment time. The same workload subsequently measured around 0.24 s specialized versus 0.21 s base, while preserving identical specialized output. A later controlled 5 MiB in-process benchmark measured the specialized path at parity within normal run-to-run noise.

## Remaining tokenizer gaps

Automatic routing and production linguistic quality are deliberately not faked. The included language packs prove composability and model-neutral specialization mechanics; they are not sufficient evidence for broad multilingual performance claims.
