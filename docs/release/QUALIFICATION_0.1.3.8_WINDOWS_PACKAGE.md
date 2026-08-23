# Mosaic 0.1.3.8 Windows Package Qualification

Status: local Windows x86-64 desktop package qualification complete.

## Package Evidence

- staged Windows install tree from `tools/package_windows_app.ps1`: **PASS**;
- warning-free Inno Setup 6 build of `MosaicCompressorSetup-0.1.3.8-x64.exe`: **PASS**;
- installer product-version metadata `0.1.3.8`: **PASS**;
- installer SHA-256 `9AA5458AA948C5616E2901CCEA6F0CC2C5655A711BAE5FC912511C23760E76CE`: **PASS**;
- package validator covering the required desktop, CLI, runtime, header, documentation, and pack payload: **PASS**;
- staged desktop self-test against the arbitrary-byte fixture with exact SHA-256 round-trip equality: **PASS**;
- installer pruning leaving only the current `0.1.3.8` executable: **PASS**;
- artifact checksum manifest refresh and deterministic check: **PASS**;
- release-readiness validation after packaging: **PASS**.

## Notes

The installer uses one stable application identity, closes the running app during upgrade, removes the previous installed application payload before copying the new staged tree, and keeps user configuration outside the installation directory intact.
