# Mosaic Tokenizer 0.15.0

Mosaic 0.15 adds source-mapped semantic enrichment over exact lexical spans and first-class sub-byte extraction.

## Added
- identifier component projection with camel/Pascal/acronym/digit boundaries;
- structured number components (radix, integer, fraction, exponent);
- string delimiter/content components;
- semantic TokenDocument density flag and capability;
- bounded 1-64 bit extraction with explicit MSB0/LSB0 numbering;
- nibble and cross-byte conformance vectors.

Semantic components are derived metadata, not a source partition. Byte source identity, lexical identity, and model tokenization are unchanged.
