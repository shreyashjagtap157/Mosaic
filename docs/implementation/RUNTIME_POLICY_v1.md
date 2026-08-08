# Runtime Policy v1

Status: stable additive runtime contract for Mosaic Tokenizer 0.19.0.

## Purpose

Runtime policy separates tokenizer semantics from deployment resource policy. The semantic tokenizer fingerprint is determined by the exact model/Unicode/language/detector/security/normalization/lexer configuration. Runtime ceilings do not alter that fingerprint. A separate runtime identity binds the semantic fingerprint to the exact policy.

## Limits

`mosaic_runtime_limits` v1 defines:

- `max_input_bytes`: maximum accepted source or decoded output bytes for high-level tokenizer operations;
- `max_output_tokens`: maximum model tokens returned by high-level encode operations;
- `max_token_document_bytes`: maximum source bytes accepted by TokenDocument materialization;
- `flags`: must be zero in v1.

All limits are non-zero. Default values are 1 GiB each. Deployments SHOULD select materially smaller workload-specific limits.

## Allocation-aware enforcement

- Input ceilings are checked before source buffering or tokenization.
- Encode token ceilings are checked after path selection but before public token-index reconstruction/output-array allocation.
- Decode byte ceilings are checked during length preflight before decoded output allocation.
- Buffered streams, editable documents, incremental documents, and resynchronizing documents snapshot the source-byte ceiling when created.
- TokenDocument creation checks both input and TokenDocument ceilings before source duplication/projection materialization.

The current Viterbi reference/engine still requires bounded working state proportional to input length for backpointers. `max_input_bytes` is therefore also the primary bound on that working memory.

## Sealing and concurrency

`mosaic_tokenizer_seal` transitions a configured tokenizer into immutable shared-runtime state. After sealing, APIs that mutate attached packs or runtime policy return `MOSAIC_ERROR_STATE`. Read/tokenization operations remain concurrent-safe when their documented output ownership rules are followed.

Child streams/documents snapshot configuration and limits at creation, and remain valid if the parent tokenizer is released.

## Identity

- `mosaic_tokenizer_fingerprint`: semantic identity only.
- `mosaic_tokenizer_runtime_identity`: SHA-256 over the semantic fingerprint plus exact v1 runtime limits/policy version.

Changing a ceiling changes runtime identity but MUST NOT change semantic fingerprint or canonical output for inputs accepted under both policies.

## Metrics

Lock-free relaxed atomic counters expose:

- encode/decode call counts;
- bytes in/out;
- tokens out;
- failures;
- resource-limit rejections.

Metrics are operational telemetry and are not part of tokenizer identity. Resetting metrics does not mutate semantics or policy.

## Failure behavior

Policy violations fail explicitly with `MOSAIC_ERROR_RESOURCE_LIMIT`. Mutation after sealing fails with `MOSAIC_ERROR_STATE`. No policy rejection may silently truncate input, token IDs, decoded bytes, or TokenDocument projections.
