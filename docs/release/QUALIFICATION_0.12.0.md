# Mosaic Tokenizer 0.12.0 Qualification

- TokenDocument model projection equals existing tokenizer IDs/start/length byte spans exactly.
- Grapheme projection equals direct Unicode projection, including malformed UTF-8.
- exact source copy and tokenizer fingerprint survive parent tokenizer destruction.
- automatic-routing TokenDocument records the exact selected language and specialized model projection.
- empty documents and unsupported projection requests are covered.
- ASan/UBSan and Clang static analysis pass.
- Release-mode CMake includes TokenDocument conformance.
- extracted release package external C client creates and uses a TokenDocument after tokenizer destruction.
- all pre-0.12 tokenizer, pack, Unicode, security, normalization, streaming, and incremental regression gates remain required.
