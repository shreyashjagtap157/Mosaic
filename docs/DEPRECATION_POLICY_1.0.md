# Mosaic 1.x Deprecation Policy

A stable 1.x API may be deprecated but not removed before the next major release except for a documented critical security exception.

## Required deprecation process

1. Mark the API/documented behavior deprecated in source documentation and release notes.
2. Provide a supported replacement when one exists.
3. Keep the old ABI symbol and behavior functional throughout 1.x.
4. Emit no unsolicited stderr/stdout warnings from library code. Bindings/tooling may surface structured deprecation warnings.
5. Add migration examples and a compatibility test covering both old and replacement APIs.
6. Remove only in the next major release, with the removal listed in its migration guide.

Experimental/research components outside the frozen stable headers/formats may follow a faster lifecycle but must be explicitly labelled experimental.
