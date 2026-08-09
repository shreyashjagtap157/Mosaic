# ADR-014: Resource Policy Schemas

Status: accepted incrementally; M2 pack-validation subset implemented  
Date: 2026-08-07

## Decision

Limits are separated into four conceptual schemas rather than a universal bag of integers:

- `PackValidationLimits`
- `RuntimeProcessingLimits`
- `ProjectionLimits`
- `SerializationLimits`

Format hard maxima and deployment policy maxima are distinct. A format may technically encode a larger count than a deployment is willing to accept.

## M2 implemented pack-validation limits

- total pack bytes;
- section count;
- individual section bytes;
- alignment exponent;
- exact dependency count;
- identity-component byte length;
- DFA states;
- DFA transitions;
- top-level manifest pack references.

M2 also rejects undeclared/unknown flags, noncanonical padding, zero dependency hashes, duplicate logical dependency identities, and out-of-bounds DFA states independently of numerical policy maxima.

## Later limits

As their formats appear, the contract will add normalization expansion, token length, candidate count per position, nesting/lookaround, graph edges, decompression output, local dictionaries, mapping fanout, language candidates, detector models, scanner-VM steps/stack, block sizes, and serialized output limits.

No feature may introduce an unbounded attacker-controlled loop simply because its limit field has not yet found a fashionable home.
