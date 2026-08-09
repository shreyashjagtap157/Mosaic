# Mosaic 0.1.2.0 Qualification

Status: post-version-bump qualification pending.

Pre-bump feature evidence:

- Python OnlineStream full/stream equivalence across arbitrary chunking: PASS;
- service batch output/order equivalence to native per-item encoding: PASS;
- batch item and decoded-byte resource limits: PASS;
- resumable stream create/push/finish exact equivalence: PASS;
- stream cancellation and missing-session rejection: PASS;
- stream pending-byte bound and zero leaked active sessions after lifecycle completion: PASS;
- service admission saturation, JSON metrics and Prometheus metrics: PASS.

All native/C API/package gates must be rerun on freshly rebuilt 0.1.2.0 artifacts before freezing this candidate.
