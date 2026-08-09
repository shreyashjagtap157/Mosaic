# Mosaic 0.1.0.3

Mosaic 0.1.0.3 is the canonical four-part re-baseline of the current enterprise candidate after real Windows qualification exposed portability defects that invalidated the earlier product-level “stable 1.x” label.

## Versioning correction

Product releases now use `S.M.N.P`:

- `S`: stability generation;
- `M`: major release;
- `N`: minor release;
- `P`: compatibility-preserving patch.

The current build remains in stability generation `0`. The first fully qualified stable baseline is reserved as `1.0.0.0`.

## Cumulative Windows patches represented by `.3`

The current candidate includes the three compatibility-preserving fixes previously tagged under the legacy three-component scheme:

1. explicit UTF-8 repository tooling and CRLF-neutral source checksums;
2. distinct Windows static-library and DLL-import-library basenames;
3. strict-warning-compatible Microsoft UCRT policy while retaining ISO C file I/O, plus removal of fixed-literal `strcpy` calls.

## Compatibility

- Native C ABI: 1.0.0, unchanged.
- Optional trust ABI: 1.0.0, unchanged.
- Tokenizer semantics: version 2, unchanged.
- MOSPACK and all frozen binary-format contracts: unchanged.
- Pack registry version constraints: continue to use ordinary three-component numeric SemVer independently of product release numbering.

Historical tags are retained. `v1.0.0` through `v1.0.3` are now documented as legacy product-version labels rather than rewritten or deleted.
