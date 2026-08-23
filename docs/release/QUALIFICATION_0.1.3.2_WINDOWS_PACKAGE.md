# Mosaic 0.1.3.2 Windows Package Qualification

Status: local Windows x86-64 desktop package qualification pending rebuild after version sync.

## Package Evidence

- staged Windows install tree created from `tools/package_windows_app.ps1`: will be revalidated after rebuild;
- installer packaging via Inno Setup 6: will be revalidated after rebuild;
- package validator `tools/validate_windows_package.py`: will be rerun after rebuild;
- release-readiness entrypoint including the Windows package gate: will be rerun after rebuild.

## Notes

This note records the Windows installer and stage layout that are part of the supported product boundary. It does not change the stable-generation status, and it does not claim macOS/ARM64 or other external qualification gates.

