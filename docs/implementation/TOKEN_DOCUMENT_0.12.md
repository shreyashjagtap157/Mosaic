# TokenDocument / Core Token IR 0.12

0.12 introduces the first immutable consumer-neutral TokenDocument snapshot.

## Invariants

- authoritative source is copied exactly and identified by SHA-256;
- tokenizer semantic/pack identity is frozen into the document fingerprint field;
- all coordinates are original byte coordinates;
- model tokens form an exact source partition and are stored internally as an 8-byte ID/length column with implicit starts;
- Unicode grapheme ranges are optional and byte-mapped;
- callers pay only for requested model/grapheme projections;
- the document remains valid after the parent tokenizer is destroyed;
- automatic-routing creation records the exact detection decision used for the model projection.

## API

`MOSAIC_TOKEN_DOCUMENT_MODEL` and `MOSAIC_TOKEN_DOCUMENT_GRAPHEMES` select the initial density. Unrequested projections return `MOSAIC_ERROR_UNSUPPORTED` rather than materializing hidden work.

This is intentionally the Core IR, not the final rich graph. Security, normalization, lexical/semantic structures, transformation provenance, capability negotiation, incremental block indexes, and serialization remain additive future projections.
