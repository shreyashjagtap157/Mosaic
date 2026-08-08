# Mosaic Tokenizer 0.12.0

0.12.0 begins the token-native platform layer with an immutable TokenDocument/Core IR snapshot over exact source bytes.

The document can retain compact model-token IDs/lengths and Unicode grapheme byte ranges, records source and tokenizer identities, supports explicit or fail-soft automatic routing, and remains valid independently of the tokenizer that created it. Projection density is explicit; unrequested views are not constructed.

No canonical model-token semantics changed. Tokenizer semantics remain version 2.
