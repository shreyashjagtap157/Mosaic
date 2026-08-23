# Benchmarks

Mosaic benchmark results must be accompanied by a completed copy of `docs/implementation/BENCHMARK_MANIFEST_TEMPLATE.toml` and exact content hashes for corpus, tokenizer manifest, packs, source commit, and baseline configuration.

No benchmark result checked into this directory may be labeled a Mosaic measurement until the corresponding code path has actually executed on recorded hardware.

Low-end desktop measurements should prefer the native `mosaic-low-memory-bench` executable and record the hardware profile explicitly in the benchmark manifest. That keeps constrained-machine evidence separate from workstation or server results.
