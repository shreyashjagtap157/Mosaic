# Security Policy

Security fixes are developed against the current `0.1.0.x` candidate line while Mosaic completes the stable-generation qualification gates. Frozen ABI/format/semantic contracts remain enforced even though the product stability-generation component is still `0`.

Critical security fixes may intentionally reject a previously accepted malicious or noncanonical artifact only through the documented security-exception process in `docs/COMPATIBILITY_POLICY.md`. Product release numbering follows `docs/VERSIONING_POLICY.md`.

The first fully qualified stable product baseline will be `1.0.0.0`; until then, the newest candidate patch is the preferred security test target.

Report suspected vulnerabilities privately through the repository's configured security-reporting channel rather than publishing exploit details in a public issue when responsible disclosure is appropriate.
