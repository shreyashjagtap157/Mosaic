# Mosaic 0.1.3.0 Windows Package Qualification

Status: local Windows x86-64 desktop package qualification complete for the staged installer path.

## Package Evidence

- staged Windows install tree created from `tools/package_windows_app.ps1`: **PASS**;
- installer packaging via Inno Setup 6: **PASS**;
- package validator `tools/validate_windows_package.py`: **PASS**;
- release-readiness entrypoint including the Windows package gate: **PASS**;
- installed package contents include:
  - desktop app;
  - desktop self-test;
  - tokenizer CLI;
  - comparison harness;
  - shared runtime binary;
  - public header;
  - release docs;
  - reference packs.

## Notes

This note records the Windows installer and stage layout that are now part of the supported product boundary. It does not change the stable-generation status, and it does not claim macOS/ARM64 or other external qualification gates.
