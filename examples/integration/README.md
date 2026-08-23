# Mosaic Integration Examples

These examples show the recommended embedding shape for other apps, agents, and desktop tools.

## C

`low_memory_embed.c` demonstrates:

- loading the native tokenizer from packs;
- applying the low-memory runtime defaults;
- sealing the tokenizer before shared use;
- using a direct encode path from C;
- handling `MOSAIC_ERROR_RESOURCE_LIMIT` as a normal bounded-environment result.

## Python

`low_memory_embed.py` demonstrates:

- loading the native library;
- enabling the low-memory defaults;
- using the sealed tokenizer and online stream APIs;
- keeping ownership inside the wrapper.

## Desktop

The Windows desktop package demonstrates:

- installing the GUI compressor as an end-user app;
- keeping a built-in self-test for non-interactive verification;
- shipping the tokenizer CLI for automation;
- bundling the public header, packs, and release docs for SDK-style reuse.
