# ADR-010: Tokenizer manifest identity

Status: accepted for M2

Canonical processing identity is `(source bytes, resolved TokenizerManifest, projection request)`. Runtime binary version alone is insufficient. A canonical manifest contains exact pack content hashes and a dependency-lock hash; mutable registry aliases are authoring/install conveniences only.
