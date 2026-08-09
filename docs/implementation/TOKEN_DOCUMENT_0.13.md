# TokenDocument Rich Projections 0.13

0.13 extends the immutable TokenDocument/Core IR without changing source truth.

## Added projections

- optional Unicode-security evidence snapshot;
- optional source-mapped normalization snapshot with explicit normalization mode;
- tokenizer capability discovery;
- extended creation options while retaining the simpler 0.12 constructors.

## Density rule

Security scanning and normalization are never performed unless requested. Getters for unrequested projections return `MOSAIC_ERROR_UNSUPPORTED`.

## Ownership

All requested views are copied into the immutable TokenDocument. They remain valid after the originating tokenizer and its packs are released.

## Qualification

The conformance test requests model + grapheme + security + NFC views over a source containing a combining sequence and bidi control. It checks exact source preservation, security evidence, NFC bytes/provenance, capability discovery, sparse-view behavior, and parent-lifetime independence under ASan/UBSan.
