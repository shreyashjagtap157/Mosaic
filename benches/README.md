# Benchmarks

Mosaic benchmark results must be accompanied by a completed copy of `docs/implementation/BENCHMARK_MANIFEST_TEMPLATE.toml` and exact content hashes for corpus, tokenizer manifest, packs, source commit, and baseline configuration.

No benchmark result checked into this directory may be labeled a Mosaic measurement until the corresponding code path has actually executed on recorded hardware.

Low-end desktop measurements should prefer the native `mosaic-low-memory-bench` executable and record the hardware profile explicitly in the benchmark manifest. That keeps constrained-machine evidence separate from workstation or server results.

For a ready-to-fill constrained example, see `benches/low_memory_4gb.toml`.
For a convenience runner that executes the benchmark and writes a filled record, use `tools/run_low_memory_profile.py`. Its default output goes under `benches/low_memory_4gb.runs/`, which is ignored by Git except for the folder marker.
For a companion machine-profile capture, use `tools/record_machine_profile.py` or `make low-memory-machine-profile`.
For one-step capture of both, use `make low-memory-evidence`.
