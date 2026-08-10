# Mosaic 0.1.3.0 Qualification

Status: local Windows x86-64 native/Python/API qualification complete for the additive span-routing surface.

## Fresh Local Qualification

- dependency-minimal fresh CMake build with Clang 19.1.5: **PASS**;
- CTest suite: **22/22 PASS**;
- detector smoke includes gapless mixed en/hi/ja span routing and span-auto exact decode preservation: **PASS**;
- native CLI `analyze-span-auto` span-routing smoke: **PASS**;
- Python binding suite against the freshly built `mosaic.dll`: **4/4 PASS**;
- product version synchronization: **PASS**;
- deterministic artifact checksum check: **PASS**;
- repository validator: **PASS**;
- frozen C ABI contract: **PASS**;
- Windows PE/COFF export-aware ABI validation against `mosaic.dll`: **PASS**;
- frozen binary-format/tokenizer-semantics contract: **PASS**.

## Source Identity

This candidate intentionally adds public C ABI surface and is therefore released as product minor `0.1.3.0`, with C ABI `1.1.0`. Tokenizer semantics remain version `2`.

No stable-generation `1.0.0.0` claim is made. Current-source macOS/ARM64, Miri/fuzz/race and final multi-platform stable-release gates remain open.
