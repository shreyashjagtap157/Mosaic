# Mosaic Tokenizer 0.8.0 Release Notes

Mosaic Tokenizer 0.8.0 adds exact source-mapped Unicode normalization shadow views while preserving the byte-authoritative tokenizer contract.

## Added

- self-contained Unicode-16 normalization MOSPACK generated offline from ICU 76.1;
- NFD, NFC, NFKD, NFKC, and NFKC-casefold mapped views;
- exact provenance arena mapping normalized output scalars back to original source byte span(s);
- algorithmic Hangul decomposition/composition;
- canonical combining-mark reordering with stable source provenance;
- malformed UTF-8 preservation as opaque normalization barriers;
- standalone and integrated normalization C APIs;
- normalization-aware tokenizer fingerprints and CLI commands;
- 10,000-case independent ICU differential conformance suite;
- 11 malformed normalization-pack fixtures plus ASan/UBSan coverage.

## Data-version boundary

Unicode segmentation/security data remains Unicode 17.0.0. The normalization pack is explicitly Unicode 16.0.0 because the locally available ICU 76.1 normalization database is Unicode 16. Mosaic does not relabel those tables as Unicode 17. Normalization stability means older assigned characters retain normalization behavior, but pack identity still records the actual source-data version.

## Defect caught before release

The first normalization pack incorrectly built canonical composition pairs from recursively decomposed NFD forms. Random ICU differential testing exposed incomplete recomposition for characters such as `ắ`. The generator now derives composition pairs from immediate raw canonical decompositions and verifies composability with ICU before emitting the deterministic pack.

## Compatibility

C API advances backward-compatibly from 0.5.0 to 0.6.0. Canonical tokenization semantics remain version 2 because normalization is an optional derived view and does not alter encode/decode when absent.
