# Mosaic Pack Authoring 0.5

`tools/mosaic_author.py` is the supported deterministic pack authoring tool shipped as `mosaic-author` in release bundles.

It is dependency-free Python and emits the same validated MOSPACK containers consumed by the native runtime.

## Compile an explicit model

Model configuration always receives all 256 byte-fallback entries automatically. User pieces may use UTF-8 text or exact hexadecimal bytes.

```json
{
  "byte_cost": 1000,
  "pieces": [
    {"text": "tokenizer", "cost": 120},
    {"hex": "00ff", "cost": 400}
  ]
}
```

```bash
mosaic-author model model.json model.mpack
```

Missing token IDs are assigned deterministically from canonical surface order. Explicit IDs, when supplied, must be unique and >=256.

## Train a compression-first model

```bash
mosaic-author train-model corpus-a.txt corpus-b.txt \
  -o model.mpack \
  --vocab-size 4096 \
  --max-piece-bytes 32 \
  --min-frequency 2 \
  --report training-report.json
```

The 0.5 trainer is deterministic and memory-bounded with respect to source records plus a configurable candidate map. It mines Unicode-aware word/punctuation/whitespace records, bounded neighboring compositions, and prefixes/suffixes for long records. Ranking uses deterministic byte-compression gain and stable tie-breaking. Serving costs are integer-only.

This is a practical baseline trainer, **not** the future constrained-Unigram research trainer. It is suitable for producing working Mosaic vocabularies and reproducible experiments; model-quality claims require downstream evaluation.

Use `--max-candidates` to fail closed before candidate memory becomes unreasonable.

## Compile a language specialization pack

```json
{
  "language": "en",
  "adjustments": [
    {"text": "tokenizer", "delta": -40}
  ]
}
```

```bash
mosaic-author language language-en.json en.mpack
```

Language packs cannot introduce token IDs. Their surfaces are projected onto an already-loaded model vocabulary at attachment time.

## Compile a detector pack

```json
{
  "min_margin": 20,
  "profiles": {
    "en": {
      "min_score": 100,
      "features": [
        {"text": "tokenizer", "weight": 120}
      ]
    }
  }
}
```

```bash
mosaic-author detector detector.json detector.mpack
```

Detector features are exact byte surfaces with positive integer weights. Low-confidence/tied results remain fail-soft in the runtime.

## Inspect a pack

```bash
mosaic-author inspect model.mpack
```

Inspection reports outer-format version, file/canonical hash state, and section directory metadata.

## Reproducibility

For equal canonical inputs/options, pack bytes are deterministic. Training corpus command-line order does not affect output; corpus records are processed in deterministic path order. No wall-clock timestamp, host path, Python hash iteration, or random seed enters canonical pack bytes.
