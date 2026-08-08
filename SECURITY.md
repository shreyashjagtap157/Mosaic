# Security Policy

## Supported releases

Security fixes are developed against the current stable 1.x line under the compatibility policy. Critical security fixes may intentionally reject a previously accepted malicious/noncanonical artifact only through the documented security-exception process. Pre-1.0 releases are development history and are not the recommended security-maintenance line after 1.0.

## Reporting vulnerabilities

Do not publish exploit details before maintainers have had a reasonable opportunity to investigate and remediate. Report privately through GitHub's private vulnerability reporting/security-advisory mechanism for the canonical repository when available.

Include the affected Mosaic version, platform, pack/manifest hashes when relevant, a minimal reproducer, expected versus observed behavior, and whether untrusted input or packs are required.

## Security boundaries

Ordinary packs are data, not native executable plugins. Structural validation is mandatory even for signed packs. Signatures establish publisher/content authenticity; they do not waive bounds/resource validation. Source bytes are never normalized or rewritten as canonical input. Runtime limits should be configured and the tokenizer sealed before serving untrusted multi-tenant workloads.

Mosaic does not claim to be vulnerability-free. Security claims are limited to specific invariants and tested failure classes.
