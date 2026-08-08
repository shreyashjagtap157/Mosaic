# Mosaic Deprecation Policy

Product release numbering follows `docs/VERSIONING_POLICY.md`. The current `0.1.x.x` candidate line already preserves the frozen public C/trust ABI and format contracts that are intended to graduate into stable generation `1`.

A frozen API may be deprecated but not removed inside a compatibility-preserving patch. Removal requires an explicitly declared compatible release boundary or a documented critical-security exception.

## Required deprecation process

1. Mark the API/documented behavior deprecated in source documentation and release notes.
2. Provide a supported replacement when one exists.
3. Keep the old ABI symbol and behavior functional throughout the compatible release line.
4. Emit no unsolicited stderr/stdout warnings from library code. Bindings/tooling may surface structured deprecation warnings.
5. Add migration examples and a compatibility test covering both old and replacement APIs.
6. Remove only through an intentional release that permits the compatibility break, with the removal listed in its migration guide.

Experimental/research components outside the frozen headers/formats may follow a faster lifecycle but must be explicitly labelled experimental.
