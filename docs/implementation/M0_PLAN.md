# M0 Engineering Substrate Plan

M0 is a real milestone, not "create a repository". Expected effort is roughly 2-4 weeks for a small team depending on CI infrastructure and review depth.

## M0 deliverables

### Repository and ownership

- Cargo workspace with dependency directions visible in crate boundaries.
- Architecture/spec documents moved under `docs/spec/` without rewriting their historical content.
- Implementation docs under `docs/implementation/`.
- ADRs under `docs/adr/`.
- Fuzz, fixtures, benchmark, and tools directories created before feature pressure distorts them.

### Governance

Initial foundational ADR sequence:

- ADR-001 Byte coordinates
- ADR-002 Canonical byte leaves
- ADR-003 Source ownership/versioning
- ADR-004 Pack binary container
- ADR-005 Pack exact identity
- ADR-006 Dependency resolution/lock graph
- ADR-007 Canonical cost representation

ADRs 008-015 are expected by M2; ADRs 016-021 by M3. This avoids pretending twenty-one documents must exist before the first `ByteRange` is written.

### CI

Three tiers:

- PR: fast correctness and structural gates.
- Nightly: broad differential/security/reproducibility.
- Release qualification: exhaustive supported-platform evidence.

See `CI_TOPOLOGY.md`.

### Benchmark substrate

- machine/environment manifest schema;
- source/corpus hash fields;
- pack/manifest hash fields;
- output density and semantic-equivalence fields;
- benchmark-family identifier B1-B4;
- warm/cold and thread-count fields.

### Fuzz substrate

Targets exist from M0 even when behavior is minimal:

- arbitrary source bytes;
- pack header;
- varint/packed decoding placeholder;
- manifest placeholder;
- streaming placeholder;
- edit-sequence placeholder.

Every discovered crash or semantic counterexample becomes permanent corpus input later.

### Reference hardware

M0 defines selection and recording policy. It does not fabricate a reference CPU that the project does not own. A real machine must be assigned before publishable performance gates are enforced.

### Empty pack fixture

`fixtures/packs/empty-v0.mpack` is a minimal structurally valid M0 fixture. It is deliberately non-executable and not a production format promise. Its purpose is to give pack parsing/fixture infrastructure a stable regression seed from day one.

## M0 exit checklist

- [x] Repository layout created.
- [x] Architecture spec organized under `docs/spec/`.
- [x] Implementation plan created.
- [x] CI topology documented and workflow skeletons created.
- [x] ADR-001 through ADR-007 drafted.
- [x] Benchmark manifest template created.
- [x] Fuzz target skeleton created.
- [x] Reference hardware policy created.
- [x] Empty pack fixture + deterministic generator created.
- [x] Structural repository validation tool created.
- [ ] Rust workspace compiles on stable toolchain.
- [ ] Rust formatter/clippy/test gates pass.

The last two remain unverified in the artifact-generation environment because no Rust toolchain is installed there. They are mandatory before M0 is considered qualified in a real development environment.
