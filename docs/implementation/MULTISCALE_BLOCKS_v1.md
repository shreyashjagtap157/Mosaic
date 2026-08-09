# Multiscale Processing Blocks v1

Status: normative for Mosaic 0.16.x.

Blocks are scheduling/cache units, never canonical token boundaries. A block boundary MUST coincide with a model-token boundary. Mosaic never splits a model token to satisfy a target block size.

Default policy:

- minimum: 16 KiB
- preferred: 64 KiB
- maximum: 256 KiB
- macroblock target: 4 MiB
- explicit maximum block and macroblock counts

Selection chooses the token boundary nearest the preferred size inside the minimum/maximum window, preferring the larger block on equal distance. If one model token itself exceeds the configured maximum, it forms one block marked `MOSAIC_BLOCK_OVERSIZE_TOKEN`.

Each processing block records source span, model-token range, source SHA-256, and a content identity `SHA256("MOSAIC-BLOCK-v1" || tokenizer_fingerprint || source_bytes)`. The ordinal source position is deliberately not part of content identity, allowing unchanged content to retain identity after movement.

Macroblock identity hashes the ordered child block identities under a separate domain and the same tokenizer fingerprint.
