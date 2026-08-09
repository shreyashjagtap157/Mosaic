**MOSAIC-µ**

**Universal Tokenization and Token-Native Processing Platform**

Extremely Exhaustive Project Specification, Implementation Options,  
Forecast Metrics, Risks, and Engineering Decision Record

Document status: Architecture and requirements baseline for research and implementation  
Version: 0.1 — Independent clean-room consolidation  
Date: 6 August 2026  
Scope: Universal platform by default; LLM-only design retained as an optional side branch

| **Evidence status** All benchmark numbers for Mosaic-µ are forecasts until a prototype is implemented and tested. Research and production systems are cited as feasibility evidence, not as proof that Mosaic-µ will reach the stated targets. |
|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

*Working project name only. The architecture does not depend on the name.*

# Document Control

| **Field**                                 | **Value**                                                                                                                                                                    |
|-------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Project**                               | Mosaic-µ Universal Tokenization and Token-Native Processing Platform                                                                                                         |
| **Scope baseline**                        | Universal tokenization, canonical Token IR, token-native processing runtime, composable packs, and consumer projections                                                      |
| **Optional branch**                       | LLM-only static tokenizer and native adaptive byte-patch architecture                                                                                                        |
| **Primary implementation recommendation** | Stable Rust; no_std-capable microkernel; stable C ABI; thin language bindings                                                                                                |
| **Normative vocabulary**                  | MUST, SHOULD, MAY are used as requirement strengths                                                                                                                          |
| **Metric status**                         | Estimated engineering targets; not measured Mosaic results                                                                                                                   |
| **Independence**                          | This document treats Mosaic-µ as an independent project and does not assume any external operating system, programming language, filesystem, AI framework, or prior project. |

## Revision History

| **Version** | **Date**   | **Status**            | **Summary**                                                                                                                                                                                                                   |
|-------------|------------|-----------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 0.1         | 2026-08-06 | Architecture baseline | Consolidates the complete conversation: microkernel, packs, Token IR, processing runtime, language specialization, LLM-only branch, unit hierarchy from bits to MiB, implementation comparisons, metrics, risks, and roadmap. |

## How to Read This Document

- **Normative requirements.** Statements labeled MUST define required behavior for a conforming implementation. SHOULD statements are recommended unless a documented trade-off justifies deviation. MAY statements are optional.

- **Recommended implementation.** The specification distinguishes architectural requirements from the current preferred implementation. Another implementation can conform if it preserves the invariants.

- **Forecast metrics.** All estimated improvements are ranges. The ranges must be validated through benchmark gates before public claims are made.

- **Brutal-honesty notes.** These identify unavoidable limits, hidden costs, or areas where the proposal may fail. They are not decorative pessimism; they are part of the engineering baseline.

## Contents

1\. Part I — Project Definition and Design Laws

2\. Part II — Core Architecture and Data Model

3\. Part III — Packs, Languages, and Domain Specialization

4\. Part IV — Tokenization Algorithms and Consumer Processing

5\. Part V — Multiscale Storage, Bit/Nibble Packing, and Incrementality

6\. Part VI — Security, Reliability, APIs, and Implementation Language

7\. Part VII — Implementation Option Comparisons and Selection

8\. Part VIII — Forecast Metrics and Benchmark Methodology

9\. Part IX — Optional LLM-Only Branch

10\. Part X — Roadmap, Testing, Governance, Risks, and Final Decision

11\. Appendices — Schemas, APIs, Requirement Traceability, and References

# Executive Summary

Mosaic-µ is proposed as a small, language-neutral tokenization microkernel surrounded by replaceable, declarative knowledge packs and a canonical token-processing contract. Its purpose is not merely to split text into model IDs. Its purpose is to preserve exact input, derive multiple compatible token views, and let compilers, IDEs, search systems, security tools, LLMs, and other consumers reuse one verified representation instead of repeatedly decoding, normalizing, scanning, and disagreeing about source boundaries.

The design deliberately separates representation from interpretation. Original bytes remain authoritative. Unicode, lexical, linguistic, semantic, and model-specific information are mapped views. A compiler may consume lexemes, an LLM may consume vocabulary IDs or dynamic patches, and a search engine may consume normalized terms, but each result maps back to the same source bytes and records its transformation provenance.

Universality comes from complete byte coverage and extensibility, not from hard-coding every language into the executable. Unicode, script, language, locale, domain, lexer, model-vocabulary, and security packs can be installed or mounted independently. English is one pack among many. Without a language pack, every input remains exactly tokenizable through byte and Unicode fallback. With a pack, segmentation quality, compression, language routing, and linguistic coherence improve.

The implementation is kept small by moving expensive generation work offline. The deployed microkernel validates and executes compact automata, tries, weighted segmentation tables, and bounded scanner bytecode. It does not train vocabularies, compile regular expressions, parse Unicode source databases, or carry every grammar and dictionary in the base binary.

| **Recommended baseline** Bytes are the canonical source unit; bits and nibbles are optional semantic and serialization units; variable-length tokens carry meaning; adaptive 16–256 KiB blocks bound work and enable incrementality; 1–16 MiB macroblocks enable scheduling, mapping, caching, and integrity; token IDs and metadata are bit-packed only outside the hot path. |
|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

## Decision Summary

| **Subsystem**            | **Recommended default**                                                       | **Why**                                                                           | **When to reject it**                                                                           |
|--------------------------|-------------------------------------------------------------------------------|-----------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------|
| Canonical source         | Immutable bytes                                                               | Exactness, interoperability, UTF-8 compatibility, zero-copy spans                 | Only for a domain whose native storage is not byte-addressable; still provide a byte bridge     |
| Sub-byte support         | Generic bit-span view with nibble convenience                                 | Handles real bit fields without doubling or octupling every text sequence         | Never reject; simply do not instantiate for ordinary text                                       |
| Runtime architecture     | Microkernel + declarative packs                                               | Small trusted core, extensibility, deterministic execution                        | Reject only if the application is permanently single-domain and plugin overhead is unacceptable |
| Core language            | Rust with no_std-capable core and C ABI                                       | Memory safety, zero-copy lifetimes, small deployments, broad interop              | Consider C for extreme targets with no Rust toolchain; Zig as a secondary implementation        |
| Lexing                   | Generated DFA + bounded scanner VM + optional parser feedback                 | Fast regular recognition plus safe handling of context-sensitive lexical features | Use a handwritten scanner only for exceptionally small or performance-critical profiles         |
| General LLM segmentation | Constrained Unigram with byte fallback; BPE compatibility                     | Global optimum, alternative segmentations, structural constraints                 | Use BPE when exact ecosystem compatibility or maximum simplicity dominates                      |
| Matching structure       | Compact DAFSA/trie on disk; optional double-array or flat expansion in memory | Small packs and fast serving                                                      | Use a plain hash map only for tiny vocabularies or prototypes                                   |
| Language support         | Composable script/language/locale/domain packs                                | Avoids English privilege and duplicate data                                       | Use one fixed monolithic pack only in a closed appliance                                        |
| Chunking                 | Semantic-aware content-defined hybrid                                         | Stable edits, bounded work, meaningful boundaries                                 | Use fixed chunks for immutable batch corpora where simplicity wins                              |
| Hot token IDs            | Aligned u16/u32                                                               | Fast random access and SIMD-friendly processing                                   | Use packed widths only when memory bandwidth dominates                                          |
| Cold token storage       | Block bit-packing + optional entropy coding                                   | 35–75% plausible size reduction                                                   | Avoid entropy coding when random access and frequent edits dominate                             |
| Hashing                  | BLAKE3 for cache/integrity; SHA-256 option for policy compatibility           | Parallel tree hashing and incremental verification                                | Use SHA-256 where compliance or ecosystem requirements mandate it                               |

## Brutally Honest One-Paragraph Assessment

| **Assessment** The architecture is implementable, but the complete vision is not a small weekend tokenizer. The microkernel can be small; the pack ecosystem, corpus curation, multilingual evaluation, model adaptation, conformance suite, bindings, registry, and security review are the expensive parts. The project will fail if it attempts to deliver every language, every compiler, every LLM mode, and every consumer in version 1. It becomes credible only if the first release freezes a narrow core, proves exactness and speed, and treats every richer feature as a separately benchmarked pack or projection. |
|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

# Part I — Project Definition and Design Laws

## 1. Project Purpose

Mosaic-µ exists to provide a common, exact, extensible substrate for tokenization and token-native processing. It addresses three recurring problems: conventional tokenizers often destroy or hide source information; each application independently repeats decoding and segmentation work; and multilingual or domain-specific support is usually baked into one static vocabulary rather than supplied as modular, auditable knowledge.

The platform must remain usable as a conventional tokenizer, but its deeper value is a verified TokenDocument that preserves the original input and supports several purpose-specific projections. The core must be small enough for embedding, while the complete platform can scale through packs, tools, and optional accelerators.

## 2. Goals

- Represent every possible byte sequence exactly, with no unknown input and no mandatory replacement characters.

- Preserve stable source mappings through normalization, segmentation, structural recognition, model projection, and incremental edits.

- Provide language-neutral baseline behavior and optional language specialization without privileging English in the core.

- Allow new languages, scripts, formats, model vocabularies, and domains to be added without modifying the runtime.

- Support conventional compiler lexing, LLM tokenization, search/indexing, IDE processing, formatting, security analysis, and custom consumers through projections.

- Keep the trusted runtime small by compiling complex rules and data into validated packs offline.

- Provide deterministic, reproducible behavior under pinned versions and manifests.

- Support streaming, incremental editing, content-addressed caching, parallel processing, and large documents.

- Use bit- and nibble-level techniques where they improve storage, transmission, metadata density, or genuine binary semantics, without degrading ordinary text processing.

- Expose realistic benchmark targets and kill criteria rather than promising universal superiority.

## 3. Non-Goals

- Mosaic-µ is not a universal parser, type checker, compiler, search engine, or LLM by itself.

- It does not claim one segmentation is semantically correct for every consumer.

- It does not guarantee that adding a language pack to a pretrained fixed-vocabulary LLM automatically creates new language knowledge.

- It does not make all languages equally compressible; orthography, morphology, corpus quality, and model vocabulary still matter.

- It does not eliminate the storage cost of dictionaries, Unicode tables, vocabularies, or model adapters. It only makes them optional and shareable.

- It does not guarantee every feature fits in a sub-megabyte deployment. The microkernel may; the installed knowledge may not.

- It does not replace full syntactic or semantic analysis where those are required.

## 4. Terminology

| **Term**                 | **Definition**                                                                                                                             |
|--------------------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| **Source bytes**         | The exact immutable or versioned byte sequence supplied by the caller. The authoritative truth for reconstruction and offsets.             |
| **Canonical leaf token** | A non-overlapping span participating in a gapless partition of the source. The concatenation of all leaves reconstructs the input exactly. |
| **Structural token**     | A higher-level node that contains or relates leaf tokens, such as an identifier, string, grapheme, word, number, or model patch.           |
| **View**                 | A derived interpretation, such as NFC normalization, lexical classification, search terms, or model IDs, mapped back to source spans.      |
| **Boundary lattice**     | A directed acyclic graph of candidate spans and boundaries with hard constraints, scores, and provenance.                                  |
| **Pack**                 | A versioned, validated, generally non-native data module containing automata, dictionaries, costs, metadata, or bounded scanner bytecode.  |
| **Projection**           | A selected consumer-facing sequence or structure derived from the TokenDocument.                                                           |
| **Processing block**     | An adaptive KiB-scale unit used to bound memory, enable caching, incremental updates, and parallel work.                                   |
| **Macroblock**           | A MiB-scale container for scheduling, memory mapping, persistence, distributed processing, and integrity trees.                            |
| **Dynamic patch**        | A variable-length model unit composed from bytes at runtime rather than selected from a permanent vocabulary.                              |
| **Trusted core**         | The minimal runtime whose correctness and security are required for every deployment.                                                      |

## 5. Normative Design Laws

> **MOS-REQ-001 \[MUST\]** The platform shall preserve the exact source bytes or an immutable reference whose integrity is cryptographically verifiable.
>
> **Rationale:** Every higher-level interpretation depends on a stable source of truth.
>
> **Verification:** Round-trip property tests over arbitrary byte sequences.
>
> **MOS-REQ-002 \[MUST\]** Canonical leaf tokens shall form a complete, ordered, gapless, non-overlapping partition of the source.
>
> **Rationale:** This is the mechanical guarantee that tokenized input remains exactly the supplied input.
>
> **Verification:** Coverage and adjacency assertions for every result.
>
> **MOS-REQ-003 \[MUST\]** Every input byte sequence shall be representable without an unknown token.
>
> **Rationale:** A 256-byte fallback alphabet provides unconditional coverage.
>
> **Verification:** Fuzz all byte values and malformed encodings.
>
> **MOS-REQ-004 \[MUST\]** Derived views shall not mutate or replace the authoritative source.
>
> **Rationale:** Normalization and semantic interpretation may be useful but must remain reversible or mapped.
>
> **Verification:** Source hash unchanged after every view operation.
>
> **MOS-REQ-005 \[MUST\]** Every derived token shall map to one or more original byte spans or be explicitly marked synthetic.
>
> **Rationale:** Consumers need provenance and safe source editing.
>
> **Verification:** Mapping completeness conformance tests.
>
> **MOS-REQ-006 \[MUST\]** Given identical input, manifest, packs, configuration, and random seed, canonical output shall be deterministic across supported platforms.
>
> **Rationale:** Caches, models, indexes, and reproducibility depend on stable behavior.
>
> **Verification:** Cross-platform golden vectors.
>
> **MOS-REQ-007 \[MUST\]** Language packs shall improve specialization but shall never be required for basic representability.
>
> **Rationale:** Universality cannot depend on pack availability.
>
> **Verification:** Remove all language packs and tokenize the same corpus losslessly.
>
> **MOS-REQ-008 \[MUST\]** Plain user text shall not create privileged control tokens through textual spelling alone.
>
> **Rationale:** This prevents protocol injection at the tokenizer boundary.
>
> **Verification:** Typed-control negative tests.
>
> **MOS-REQ-009 \[MUST\]** Ordinary packs shall not contain native executable code.
>
> **Rationale:** Native plugins expand the attack surface and break portability.
>
> **Verification:** Pack schema validation and executable-section rejection.
>
> **MOS-REQ-010 \[MUST\]** The scanner VM, when enabled, shall be bounded in stack, lookaround, steps per input unit, nesting, and allocation.
>
> **Rationale:** Untrusted profiles must not create unbounded execution.
>
> **Verification:** Static verifier plus adversarial packs.
>
> **MOS-REQ-011 \[MUST\]** Incremental processing shall be semantically equivalent to complete reprocessing under the same configuration.
>
> **Rationale:** Performance cannot change meaning.
>
> **Verification:** Differential edit tests.
>
> **MOS-REQ-012 \[MUST\]** Streaming and complete-buffer processing shall converge to the same canonical result when end of input is known.
>
> **Rationale:** Chunk boundaries must not alter tokenization.
>
> **Verification:** Random chunk-boundary differential tests.
>
> **MOS-REQ-013 \[SHOULD\]** The hot path should avoid copied token strings and use source spans.
>
> **Rationale:** Zero-copy operation reduces memory and consistency bugs.
>
> **Verification:** Allocation and copy profiling.
>
> **MOS-REQ-014 \[SHOULD\]** The core should compile in a no_std-capable configuration with optional allocation.
>
> **Rationale:** This supports embedded and restricted environments.
>
> **Verification:** CI targets for core-only builds.
>
> **MOS-REQ-015 \[SHOULD\]** Packs should be memory-mappable and independently loadable.
>
> **Rationale:** Applications should pay only for used capabilities.
>
> **Verification:** Mapped-load tests and residency measurements.
>
> **MOS-REQ-016 \[SHOULD\]** The platform should support explicit, preferred, document-level automatic, and span-level automatic language selection.
>
> **Rationale:** Applications know varying amounts of language context.
>
> **Verification:** Routing benchmark by mode.
>
> **MOS-REQ-017 \[SHOULD\]** Bit and nibble semantics should be represented as sub-byte views over byte-addressed source.
>
> **Rationale:** This preserves compatibility while supporting packed formats.
>
> **Verification:** Binary-format conformance tests.
>
> **MOS-REQ-018 \[SHOULD\]** KiB processing blocks should align to strong semantic or restart boundaries when possible.
>
> **Rationale:** Fixed cuts harm Unicode, lexing, and edit stability.
>
> **Verification:** Boundary-quality and edit-locality metrics.
>
> **MOS-REQ-019 \[MAY\]** Token IDs and metadata may be bit-packed or entropy-coded in cold storage and transport.
>
> **Rationale:** This reduces footprint without slowing the hot representation.
>
> **Verification:** Compression ratio and decode-throughput benchmarks.
>
> **MOS-REQ-020 \[MAY\]** An LLM-native deployment may replace fixed vocabulary IDs with dynamic byte patches.
>
> **Rationale:** Model co-design can remove vocabulary limits and allocate compute adaptively.
>
> **Verification:** Model-level quality and compute evaluation.

## 6. Formal Exactness Invariants

> For source length N and canonical leaves T\[0..k-1\]:  
>   
> T\[0\].start = 0  
> T\[i\].end = T\[i+1\].start for all 0 \<= i \< k-1  
> T\[k-1\].end = N  
> T\[i\].start \<= T\[i\].end  
> concat(source\[T\[i\].start:T\[i\].end\]) = source  
>   
> For any derived token D:  
> D.source_spans is non-empty OR D.synthetic = true

These invariants are more important than the choice between BPE and Unigram. A fast segmentation algorithm built on a lossy source model is merely an efficient way to make later disagreement irreversible.

# Part II — Core Architecture and Data Model

## 7. System Decomposition

> OFFLINE TOOLCHAIN DEPLOYED RUNTIME  
> ---------------- ----------------  
> Unicode data compiler Input cursor and source manager  
> Lexer/profile compiler Pack loader and verifier  
> Vocabulary trainer DFA/transducer executor  
> Pack optimizer Bounded scanner VM  
> Language-pack builder Candidate matcher  
> Conformance-vector generator Segmentation selector  
> Benchmark and corpus tools Token IR / projection engine  
> Pack signer and registry tools Incremental block manager

The deployed runtime is intentionally an executor, not a generator. Expensive corpus statistics, automaton determinization, Unicode table construction, dictionary minimization, and model-vocabulary training happen offline. This is the primary mechanism for keeping the trusted core small.

## 8. Runtime Microkernel Components

| **Component**           | **Responsibility**                                                                                                                 |
|-------------------------|------------------------------------------------------------------------------------------------------------------------------------|
| **Source manager**      | Borrowed buffers, owned immutable buffers, memory maps, streams, ropes, and piece tables. Maintains version and source hash.       |
| **Input cursor**        | Sequential and random access, bounded lookahead, partial UTF-8 state, streaming suffix retention, and restart checkpoints.         |
| **Pack loader**         | Validates section sizes, state targets, instruction bounds, dependencies, hashes, signatures, Unicode versions, and policy limits. |
| **Automaton executor**  | Runs compact DFAs, deterministic transducers, range maps, and boundary machines.                                                   |
| **Scanner VM**          | Handles bounded stateful features such as nested comments, indentation, heredocs, raw delimiters, and interpolation.               |
| **Candidate matcher**   | Enumerates vocabulary, dictionary, and semantic candidates from tries, DAFSAs, or fast expanded tables.                            |
| **Lattice selector**    | Applies hard constraints and chooses greedy, Viterbi, N-best, or sampled paths.                                                    |
| **Token IR manager**    | Stores leaves, structures, mappings, payloads, views, and provenance with selectable density.                                      |
| **Incremental manager** | Tracks blocks, restart states, hashes, invalidation, re-synchronization, and consumer edit events.                                 |
| **Projection engine**   | Produces IDs, lexemes, search terms, model tensors, security events, or custom cursors.                                            |

## 9. Source and Unit Hierarchy

> Level -1 Optional bit spans and nibble views  
> Level 0 Canonical bytes  
> Level 1 Graphemes, lexemes, morphemes, binary fields  
> Level 2 Consumer tokens or model patches  
> Level 3 Adaptive processing blocks (16-256 KiB typical)  
> Level 4 Macroblocks (1-16 MiB typical)  
> Level 5 Complete document, stream, or corpus object

### 9.1 Why bytes remain canonical

Files, memory, networks, operating-system APIs, UTF-8, and existing compiler offsets are byte-oriented. A byte alphabet contains only 256 values and already represents every digital input. Moving the canonical primitive downward to bits or nibbles increases sequence length without improving coverage; moving it upward to KiB or MiB makes the primitive alphabet impossible to enumerate and destroys edit locality.

### 9.2 Bit and nibble roles

Bits and nibbles remain first-class where they carry real meaning or improve serialization. A generic SubByteSpan represents fields with arbitrary bit offsets and lengths. Nibbles are a convenience for 4-bit quantities such as packed decimal digits, quantized values, hardware fields, and compact metadata enums. They are not universal text primitives.

> SubByteSpan {  
> source_byte_start: u64,  
> start_bit: u8, // 0..7  
> bit_length: u32,  
> bit_order: BitOrder,  
> byte_endianness: Endianness,  
> semantic_type: TypeId,  
> }

### 9.3 KiB and MiB roles

KiB and MiB are operational scales, not semantic alphabets. Adaptive KiB blocks bound working memory and enable parallelism, cache reuse, incremental retokenization, and resource limits. MiB macroblocks group them for memory mapping, NUMA placement, persistent storage, GPU transfer, and distributed scheduling.

### 9.4 Primitive-unit comparison

| **Candidate primitive** | **Coverage**                                                    | **Performance effect**                                                 | **Useful role**                              | **Decision**                                 |
|-------------------------|-----------------------------------------------------------------|------------------------------------------------------------------------|----------------------------------------------|----------------------------------------------|
| Bit                     | Complete                                                        | 8x primitive sequence; estimated 40-85% lower ordinary-text throughput | True bit fields; entropy-coded storage       | Do not use as canonical text primitive       |
| Nibble                  | Complete                                                        | 2x primitive sequence; estimated 15-50% lower ordinary-text throughput | 4-bit fields, BCD, metadata packing          | Optional semantic/serialization unit         |
| Byte                    | Complete                                                        | Best compatibility and practical baseline                              | Canonical source, fallback alphabet, offsets | Selected                                     |
| Unicode scalar          | Invalid bytes become awkward; not complete without escape layer | Good text semantics, extra decode layer                                | Unicode view only                            | Not canonical                                |
| Grapheme                | Text-focused; binary and malformed input awkward                | Good human boundaries, version-dependent                               | Candidate boundary and structural token      | View, not source primitive                   |
| Word/morpheme           | Language-dependent and ambiguous                                | Potentially short model sequences, poor universality                   | Language-pack candidates                     | Derived only                                 |
| KiB/MiB chunk           | Complete only by embedding payload or reference                 | Poor edit stability if atomic; impossible static alphabet              | Processing and storage container             | Selected as block scale, not token primitive |

## 10. Source Storage Models

| **Model**                | **Advantages**                                 | **Costs and risks**                                      | **Estimated effect**                         | **Recommended use**                |
|--------------------------|------------------------------------------------|----------------------------------------------------------|----------------------------------------------|------------------------------------|
| Owned contiguous buffer  | Simple; fast random access; easy FFI           | Copies input; poor very-large-edit behavior              | Best small-input latency; 1x memory copy     | Default for small immutable inputs |
| Borrowed immutable slice | Zero-copy; minimal memory                      | Caller lifetime contract; harder across async boundaries | 20-50% lower temporary memory                | Default library API                |
| Memory-mapped file       | Low startup cost; OS paging; partial access    | Page faults; file mutation hazards; platform differences | 10-1000x less data loaded for partial access | Large immutable files and packs    |
| Rope                     | Efficient edits and substrings                 | Complex traversal; fragment overhead                     | 10-500x edit improvement versus full copies  | Editors and live documents         |
| Piece table              | Excellent edit history and source preservation | Compaction and fragmentation management                  | Similar incremental benefits to ropes        | IDEs and document tools            |
| Chunked stream           | Bounded memory; network-friendly               | Must retain partial units and mappings                   | O(block) active memory                       | Logs, sockets, dataset ingestion   |

| **Decision** Support all through one SourceProvider trait. Do not force ropes into the embedded build or copy every memory map into a Vec merely for API uniformity. |
|----------------------------------------------------------------------------------------------------------------------------------------------------------------------|

## 11. Unicode Layer

For valid Unicode regions, Mosaic-µ derives scalar values, extended grapheme clusters, script properties, default word and sentence boundaries, bidi properties, joining behavior, and security metadata. Unicode Standard Annex \#29 defines default grapheme, word, and sentence segmentation and explicitly notes that language and script conventions may require tailoring \[R1\]. Unicode Technical Standard \#39 defines mechanisms for identifying possible security problems such as confusable and mixed-script text \[R2\].

### 11.1 Invalid encodings

Invalid UTF-8 does not cause data loss. The source is partitioned into valid Unicode regions and opaque byte regions. Unicode-dependent views can skip or mark opaque regions, while byte projections remain exact.

### 11.2 Shadow normalization

Normalization produces mapped secondary views rather than rewriting source. Supported views may include NFC, NFD, NFKC, NFKD, NFKC casefold, and application-defined transforms. Destructive compatibility normalization is prohibited in the authoritative representation because it may collapse meaningful distinctions, especially in source code, identifiers, and forensic workflows.

### 11.3 Unicode implementation alternatives

| **Option**                          | **Core size**           | **Speed**             | **Updateability** | **Risk**                                                | **Decision**                                     |
|-------------------------------------|-------------------------|-----------------------|-------------------|---------------------------------------------------------|--------------------------------------------------|
| Depend directly on ICU              | Large shared dependency | High and mature       | ICU release cycle | Deployment size and behavior breadth exceed a tiny core | Optional compatibility backend, not default core |
| Use generated compact Mosaic tables | Small, selectable packs | Potentially very high | Pack update only  | Generator correctness becomes critical                  | Selected default                                 |
| Use language-native Unicode crates  | Moderate                | Good                  | Crate updates     | Version fragmentation and inconsistent coverage         | Useful prototype/reference backend               |
| Handwrite only UTF-8 and ASCII      | Tiny                    | Very high             | Simple            | Not comprehensive; multilingual failure                 | Tier-0 minimal mode only                         |

## 12. Boundary Lattice

The boundary lattice records candidate spans before a consumer projection commits to one segmentation. Vertices are source positions; edges are candidate tokens or structures. Edges carry type, score, source, hard constraints, and optional vocabulary IDs. This allows a compiler to preserve one identifier while an LLM or search projection sees its internal components.

> BoundaryStrength = { MANDATORY, STRONG, PREFERRED, NEUTRAL, DISCOURAGED, FORBIDDEN }  
>   
> CandidateEdge {  
> start, end, namespace, kind, optional_id,  
> base_cost, boundary_cost, language_cost,  
> structural_flags, source_pack, payload_ref  
> }

### 12.1 Hard constraints before scores

Losslessness, grapheme integrity, security policy, explicit grammar restrictions, and typed protocol boundaries must be applied before statistical costs. A frequent vocabulary fragment must not merge across a forbidden string delimiter or reinterpret user text as a control token.

### 12.2 Selection algorithms

| **Algorithm**               | **Strengths**                                 | **Weaknesses**                                      | **Estimated runtime**                   | **Recommended use**                            |
|-----------------------------|-----------------------------------------------|-----------------------------------------------------|-----------------------------------------|------------------------------------------------|
| Greedy longest match        | Tiny and fast; deterministic                  | Can miss globally better segmentation               | 1.0x baseline                           | Compatibility profiles and constrained devices |
| BPE merge ranks             | Ecosystem compatibility; fast implementations | Static merge history; boundary integration awkward  | 0.9-1.1x greedy                         | Existing model compatibility                   |
| Viterbi / DAG shortest path | Global optimum; clean weighted constraints    | Requires candidate lattice and backpointers         | 1.2-2.5x greedy depending on candidates | Default greenfield static tokenizer            |
| Beam search                 | Context-sensitive choices                     | More compute; reproducibility and tuning complexity | 2-20x greedy                            | Experimental contextual tokenization           |
| N-best / sampling           | Robust training and regularization            | Not deterministic without seed                      | 1.5-5x canonical Viterbi                | Training only                                  |

## 13. Canonical Token IR

Token IR is the consumer-neutral representation that closes the gap between tokenization and downstream processing. Applications receive a verified TokenDocument rather than an unqualified integer array. The model is graph-capable but should be stored as packed arrays and indexes, not pointer-rich heap objects.

> TokenDocument {  
> Header header;  
> SourceRef source;  
> LeafToken\[\] leaves;  
> StructuralNode\[\] structures;  
> ViewDescriptor\[\] views;  
> Mapping\[\] mappings;  
> PayloadArena payloads;  
> TransformationRecord\[\] provenance;  
> BlockIndex blocks;  
> }  
>   
> Header {  
> schema_version, source_length, source_hash,  
> tokenizer_manifest_hash, unicode_version,  
> profile_ids\[\], feature_flags  
> }

### 13.1 Density profiles

| **Profile**   | **Contents**                        | **Approximate output size**              | **Use**                                 |
|---------------|-------------------------------------|------------------------------------------|-----------------------------------------|
| IDs only      | Token IDs                           | 4 bytes/token with u32; less when packed | LLM hot path                            |
| IDs + lengths | IDs and compact source lengths      | 5-8 bytes/token                          | Streaming decode and alignment          |
| IDs + offsets | IDs, starts, lengths                | 7-12 bytes/token                         | Model explainability and source mapping |
| Lexical       | Kinds, spans, flags, payload refs   | 12-20 bytes/token                        | Compilers and IDEs                      |
| Rich          | Structures, mappings, metadata refs | 16-32 bytes/token plus payloads          | Search, analysis, multimodal tools      |
| Full lattice  | Alternative edges and scores        | 24-64 bytes/node/edge                    | Training, debugging, research           |

## 14. Universal Token-Processing Runtime

The processing runtime standardizes operations over the TokenDocument while leaving application semantics different. Universality applies to transport, source identity, views, mappings, versions, iteration, querying, and incremental updates. A compiler and LLM do not run the same algorithm, but they no longer need to independently reinterpret the original bytes.

- Token and span iteration without copied strings

- Source slicing and exact decoding

- Pattern matching over token kinds, namespaces, and attributes

- Grouping, filtering, mapping, and view projection

- Source-to-derived and derived-to-source coordinate mapping

- Block-level hashing, caching, and validation

- Incremental edit application and consumer invalidation events

- Capability negotiation and projection selection

- Transformation provenance and audit output

### 14.1 What remains consumer-specific

- Parsing, type checking, code generation, and optimization

- Language-model inference and generation

- Search ranking and retrieval scoring

- Security policy decisions beyond token-level findings

- Business rules, domain models, and user-interface behavior

| **Core distinction** Same source and Token IR; different task semantics. Standardizing every downstream algorithm would not be universality. It would be an unusually elaborate method of making the platform unusable. |
|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

# Part III — Packs, Languages, and Domain Specialization

## 15. Pack Taxonomy

| **Pack class**   | **Typical contents**                                                        | **Dependency pattern**           | **Primary benefit**                   |
|------------------|-----------------------------------------------------------------------------|----------------------------------|---------------------------------------|
| Unicode base     | Character properties, grapheme/word/sentence boundaries, normalization maps | Runtime only                     | Common Unicode correctness            |
| Script           | Script-specific classes, joining, punctuation, grapheme tailoring           | Unicode base                     | Shared logic across several languages |
| Language         | Boundary rules, morphology, abbreviations, dictionaries, token costs        | Unicode + script                 | Language specialization               |
| Locale           | Number/date/currency conventions, spelling variants, quotation forms        | Language                         | Regional overlay                      |
| Domain           | Medical, legal, software, scientific terminology and patterns               | Language or universal            | Specialist compression and structure  |
| Lexer/profile    | Lexical DFA, scanner VM bytecode, keywords, restart states                  | Unicode optional                 | Programming and structured formats    |
| Model vocabulary | IDs, token bytes, costs/ranks, controls, compatibility hash                 | Optional language/domain packs   | Fixed-vocabulary LLM projection       |
| Model adapter    | Embeddings, output rows, adapters, patch hints                              | Specific model and pack versions | Language or vocabulary extension      |
| Security         | Confusables, identifier policy, bidi and invisible controls                 | Unicode base                     | Audit and policy evidence             |
| Detector         | Script/language n-grams or compact classifier data                          | Unicode + language catalog       | Automatic routing                     |

## 16. Language-Neutral Core

The core must not assume spaces identify words, Latin script implies English, apostrophes follow English contractions, or case transitions carry one universal meaning. English is a language pack, not the default ontology of text. Unicode default rules provide a baseline, but UAX \#29 states that segmentation may need language-specific tailoring \[R1\]. ICU uses dictionary-backed boundary handling for languages and scripts where spaces are insufficient, demonstrating the feasibility of data-driven specialization \[R3\].

## 17. Language Pack Behavior

A language pack contributes candidate boundaries, morphology, abbreviations, dictionaries, frequency costs, detection evidence, and conformance tests. It cannot remove byte fallback, mutate source, or bypass security and resource limits. Several language packs may contribute to one mixed-language document.

> LanguagePackManifest {  
> pack_id, version, language_tags\[\], scripts\[\],  
> unicode_requirement, dependencies\[\],  
> boundary_tables, morphology_tables, dictionary,  
> candidate_costs, detector_data, security_profile,  
> quality_level, content_hash, signature  
> }

### 17.1 Support quality levels

| **Level**           | **Capability**                              | **Guarantee**                     |
|---------------------|---------------------------------------------|-----------------------------------|
| 0 — Universal byte  | Exact arbitrary bytes                       | Representability only             |
| 1 — Unicode         | Graphemes and default boundaries            | Unicode baseline                  |
| 2 — Script-aware    | Script-specific tailoring                   | Improved structural integrity     |
| 3 — Language-aware  | Language rules and statistics               | Better boundaries and compression |
| 4 — Linguistic      | Dictionaries, morphology, abbreviations     | High-quality segmentation         |
| 5 — Model-optimized | Model-specific vocabulary costs or adapters | Maximum model efficiency          |

This tiered support is deliberately honest. CLDR similarly distinguishes levels of language support rather than pretending every locale has equal depth from the first release \[R4\].

### 17.2 Routing modes

| **Mode**           | **Speed** | **Accuracy**                           | **Use case**                                   | **Estimated overhead** |
|--------------------|-----------|----------------------------------------|------------------------------------------------|------------------------|
| Explicit           | Highest   | Highest when metadata is correct       | Known language or API contract                 | 1-5% pack execution    |
| Preferred list     | High      | High                                   | Multilingual product with known user languages | 2-8%                   |
| Automatic document | Medium    | High for monolingual input             | Unknown documents                              | 3-10%                  |
| Automatic span     | Lowest    | Variable for short/code-switched spans | Chat and mixed documents                       | 10-35%                 |
| No detection       | Highest   | Generic only                           | Constrained devices or unknown/binary input    | 0% detection overhead  |

### 17.3 Detection alternatives

| **Approach**              | **Pack size**              | **Accuracy**                   | **Latency** | **Risks**                               | **Decision**                |
|---------------------------|----------------------------|--------------------------------|-------------|-----------------------------------------|-----------------------------|
| Script-only               | Tiny                       | Poor for shared scripts        | Very low    | Latin does not mean English             | Use as first filter only    |
| Rules + function words    | Small                      | Good for common languages      | Low         | Brittle on short/noisy text             | Supplementary               |
| Character n-gram model    | 10-250 KiB/language family | Strong document-level baseline | Low         | Corpus bias; short-span uncertainty     | Selected default detector   |
| Compact neural classifier | 0.5-20 MiB                 | Potentially best               | Medium      | Runtime/model dependency; opaque errors | Optional high-accuracy pack |
| Application metadata      | None                       | Best when correct              | None        | Caller may be wrong                     | Highest-priority evidence   |

## 18. Mixed-Language Composition

Language packs are composable candidate providers, not mutually exclusive cartridges. A Hindi-English technical sentence may receive evidence from Devanagari, Hindi, English, software-domain, code-lexer, and model-vocabulary packs. The selector uses explicit metadata and high-confidence boundaries first, then statistical costs.

| **Failure behavior** Incorrect language routing may reduce segmentation efficiency, but it must never make input unencodable or alter source bytes. This is the safety valve that permits aggressive specialization without sacrificing universality. |
|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

## 19. Pack Execution Models

| **Model**                   | **Performance**           | **Security**                     | **Portability**   | **Expressiveness**             | **Recommendation**                          |
|-----------------------------|---------------------------|----------------------------------|-------------------|--------------------------------|---------------------------------------------|
| Native DLL/SO plugin        | Potentially highest       | Worst; arbitrary code            | Platform-specific | Unlimited                      | Disallow by default; privileged opt-in only |
| WebAssembly module          | Good                      | Sandboxable but runtime-heavy    | High              | High                           | Optional for advanced external plugins      |
| Bounded custom VM           | Good and predictable      | Strong with verifier and budgets | High              | Moderate                       | Selected for exceptional scanners           |
| Declarative automata/tables | Highest predictable speed | Best                             | Highest           | Regular and table-driven logic | Selected default                            |
| Embedded scripting language | Moderate to poor          | Complex sandbox                  | Medium            | High                           | Reject for trusted core                     |

## 20. Pack File Format

The format should be sectioned, endian-defined, versioned, checksummed, and memory-mappable. Every section uses explicit offsets and lengths validated before access. No raw host pointers or serialized language objects are permitted.

> MOSAIC PACK  
> Header  
> Dependency table  
> String/blob pool  
> Character-class ranges  
> DFA/transducer tables  
> Trie/DAFSA sections  
> Cost and metadata columns  
> Optional scanner bytecode  
> Conformance vectors  
> Build provenance  
> Content hash  
> Optional publisher signature

### 20.1 Pack size targets

| **Pack type**          | **Compact target** | **Fast target** | **Primary size driver**                |
|------------------------|--------------------|-----------------|----------------------------------------|
| Unicode segmentation   | 100-500 KiB        | 300 KiB-1 MiB   | Property ranges and transition tables  |
| Unicode normalization  | 300 KiB-1.5 MiB    | 1-3 MiB         | Decomposition and composition mappings |
| Unicode security       | 300 KiB-2 MiB      | 1-4 MiB         | Confusable and identifier data         |
| Script pack            | 10-250 KiB         | 30-700 KiB      | Tailored properties and rules          |
| Simple language        | 50-750 KiB         | 150 KiB-2 MiB   | Rules, abbreviations, candidate costs  |
| Morphological language | 250 KiB-4 MiB      | 1-8 MiB         | Affixes, lexicon, weighted structures  |
| Dictionary language    | 1-15 MiB           | 3-30 MiB        | Word dictionary and statistics         |
| 64K model vocabulary   | 1-5 MiB            | 3-10 MiB        | Surface blob and matching automaton    |
| 128K model vocabulary  | 3-10 MiB           | 6-20 MiB        | Surface blob and expanded matcher      |

## 21. Pack Registry and External Storage

Packs may be installed, mounted directly from external storage, or fetched from a registry and cached. Every resolved pack identity includes the pack ID, semantic version, content hash, dependency hashes, Unicode requirement, and publisher signature status. A mutable name such as latest must not be the reproducibility identity.

- Installed mode: copied into a managed pack directory.

- Mounted mode: validated and memory-mapped directly from removable or external storage.

- Remote mode: fetched, verified, cached, and pinned by content hash.

- Hot/cold sections: small automata remain resident; large rare dictionaries page in on demand.

- Eviction: packs can be released when no live TokenDocument or projection depends on them.

# Part IV — Tokenization Algorithms and Consumer Processing

## 22. Compiler and Structured-Format Lexing

Compiler profiles use conventional lexical analysis before model compression. Generated DFAs handle regular rules efficiently; lexical modes and a bounded scanner VM handle stateful features; parser feedback may restrict valid candidates where a language requires context-aware lexing. re2c demonstrates UTF-8-capable generated DFA lexers across several target languages \[R9\]. Tree-sitter demonstrates practical context-aware lexing and incremental parsing robust enough for interactive editors \[R8\].

### 22.1 Lexical token classes

> KEYWORD, IDENTIFIER, TYPE_IDENTIFIER, INTEGER_LITERAL, FLOAT_LITERAL,  
> STRING_PREFIX, STRING_DELIMITER, STRING_CONTENT, ESCAPE_SEQUENCE,  
> INTERPOLATION_START, INTERPOLATION_END, OPERATOR, PUNCTUATION,  
> COMMENT, DOC_COMMENT, WHITESPACE, NEWLINE, INDENT, DEDENT,  
> PREPROCESSOR_DIRECTIVE, ERROR_TOKEN

### 22.2 Lexing implementation comparison

| **Option**                | **Throughput**      | **Binary size**             | **Correctness/expressiveness**               | **Risk**                                         | **Decision**                            |
|---------------------------|---------------------|-----------------------------|----------------------------------------------|--------------------------------------------------|-----------------------------------------|
| Backtracking regex engine | Low to medium       | Dependency-dependent        | Flexible but pathological cases possible     | Catastrophic backtracking and hidden allocations | Reject in hot core                      |
| Handwritten scanner       | Potentially highest | Small per language          | Excellent when carefully written             | Maintenance and inconsistency across packs       | Allow for privileged reference profiles |
| Generated DFA             | Very high           | Tables/code can grow        | Strong for regular lexical rules             | State explosion if generation is careless        | Selected primary                        |
| Parser-combinators        | Low to medium       | Moderate                    | Readable and composable                      | Allocation and backtracking costs                | Tooling/prototype only                  |
| DFA + bounded scanner VM  | High                | Compact shared runtime      | Handles practical context-sensitive features | VM verifier complexity                           | Selected full solution                  |
| DFA + parser feedback     | High                | Requires parser integration | Best contextual disambiguation               | Consumer coupling                                | Optional advanced mode                  |

## 23. Semantic Enrichment

### 23.1 Identifiers

Identifiers remain one lexical token but may expose components based on snake case, camel case, Pascal case, acronym transitions, digits, scripts, dictionaries, and language morphology. Compiler identity is never changed by a model-oriented decomposition.

> deserializeHTTP2Response  
> IDENTIFIER \[source span\]  
> deserialize  
> HTTP  
> 2  
> Response

### 23.2 Numbers

Numbers receive structured payloads describing sign, radix, digits, separators, fraction, exponent, suffix, unit, exact integer/decimal value when possible, and conversion status. Surface spelling remains available for exact reconstruction and generation.

> NumberPayload {  
> sign, radix, integer_digits, fraction_digits,  
> exponent_sign, exponent_digits, separators,  
> type_suffix, unit_suffix, exact_value, conversion_status  
> }

### 23.3 Strings and embedded languages

String prefixes, delimiters, content, escapes, interpolation, and embedded-language regions are represented separately while preserving the complete original literal. Nested profiles support Markdown code fences, HTML/JavaScript/CSS, SQL in strings, templates, notebooks, and documentation comments.

## 24. Static LLM Tokenization

For conventional Transformers with a fixed embedding table, Mosaic-µ should provide a model-specific projection. The greenfield recommendation is constrained Unigram segmentation with complete byte fallback, structural boundary costs, balanced multilingual training, and deterministic Viterbi decoding. BPE remains required for compatibility. SentencePiece demonstrates practical BPE and Unigram support, raw-text training, segmentation sampling, and a reported 50,000 sentences per second with about a 6 MB footprint \[R5\].

### 24.1 Vocabulary algorithm comparison

| **Algorithm**      | **Compression**                | **Robustness**                           | **Constraint integration**   | **Serving speed**                          | **Best use**                             |
|--------------------|--------------------------------|------------------------------------------|------------------------------|--------------------------------------------|------------------------------------------|
| BPE                | Strong                         | Moderate; sensitive to merge history     | Possible but awkward         | Excellent                                  | Existing ecosystem and simple deployment |
| WordPiece          | Strong                         | Moderate                                 | Pre-tokenizer-dependent      | Excellent                                  | Compatibility with WordPiece models      |
| Unigram            | Strong                         | High with alternative segmentations      | Natural weighted-lattice fit | High                                       | Recommended greenfield fixed vocabulary  |
| Character/grapheme | Weak sequence compression      | High coverage                            | Excellent                    | Very high tokenizer speed; model cost high | Small models and robust fallbacks        |
| Byte vocabulary    | No unknowns; long raw sequence | Very high                                | Excellent                    | Tokenizer trivial; model cost high         | Fallback and byte-native models          |
| Dynamic patches    | Adaptive global compression    | Potentially highest long-tail robustness | Learned/model-coupled        | Tokenization moves into model              | New co-designed LLMs                     |

### 24.2 Vocabulary training objectives

> Score(piece) =  
> + compression_gain  
> + boundary_alignment  
> + source_diversity  
> + language/domain utility  
> + expected utilization  
> - source_concentration  
> - token_instability  
> - cross-language fertility disparity  
> - structural violations

Frequency alone is insufficient. A fragment repeated in one generated repository should not displace a broadly useful piece appearing across many independent languages or sources. Corpus balancing, deduplication, language budgets, source entropy, and tail-language evaluation are mandatory parts of vocabulary training.

## 25. Candidate Matching Structures

| **Structure**                   | **Lookup speed**                           | **Storage**          | **Build cost**     | **Updateability**    | **Decision**                             |
|---------------------------------|--------------------------------------------|----------------------|--------------------|----------------------|------------------------------------------|
| Hash map by token bytes         | Good exact lookup; poor prefix enumeration | Moderate to high     | Low                | Easy                 | Prototype or decoder table only          |
| Pointer trie                    | Good                                       | High due to pointers | Low                | Easy                 | Avoid in production packs                |
| Radix trie                      | Very good                                  | Moderate             | Moderate           | Moderate             | Good general in-memory option            |
| Double-array trie               | Excellent cache locality                   | Moderate             | Higher and tricky  | Poor dynamic updates | Fast serving option                      |
| DAFSA/minimal acyclic automaton | Very compact                               | Best compact storage | High offline build | Immutable            | Selected compact pack form               |
| Full finite-state transducer    | Powerful composition                       | Can be huge          | High               | Immutable            | Use selectively for constrained decoding |

| **Recommended dual layout** Store compact DAFSA/trie sections on disk. Expand selected hot sections into a double-array or flat transition table when memory and repeated use justify the cost. |
|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

## 26. Consumer Projections

| **Consumer** | **Projection**                                          | **What is shared**                         | **What remains specific**            |
|--------------|---------------------------------------------------------|--------------------------------------------|--------------------------------------|
| Compiler     | Lexical token cursor with trivia and literal payloads   | Source, offsets, Unicode, lexical spans    | Parser, semantics, optimization      |
| IDE          | Lexical/structural tokens and incremental edits         | Source, blocks, mappings, language regions | UI, semantic services, project model |
| Formatter    | Lexemes, whitespace, comments, delimiters               | Exact trivia and spans                     | Formatting policy                    |
| Search       | Normalized terms, identifier parts, structured literals | Source mapping and language metadata       | Ranking, index design, retrieval     |
| Security     | Unicode/security findings and exact spans               | Source, scripts, confusables, controls     | Policy and enforcement               |
| LLM static   | Model IDs plus optional side channels                   | Source and structural features             | Embedding table and model            |
| LLM native   | Bytes, controls, patch hints                            | External byte protocol                     | Local encoder, patcher, decoder      |
| Custom app   | Typed token query API                                   | Token IR and provenance                    | Application semantics                |

## 27. Capability Negotiation

> ConsumerCapabilities {  
> token_ir_versions\[\], accepted_views\[\], accepted_profiles\[\],  
> payload_types\[\], supports_incremental, supports_external_source,  
> maximum_token_size, maximum_block_size, control_protocols\[\]  
> }

The runtime selects the least expensive projection satisfying the declared capabilities. An LLM that only needs IDs should not pay to construct security findings or a full structural graph. A compiler that needs trivia should not receive a lossy model-normalized stream.

## 28. Typed Control Protocol

Model and application control events are out-of-band typed records. User bytes that spell a control marker remain user bytes. The API distinguishes encode_text from emit_control. This prevents plain text from impersonating system, tool, document, or message boundaries.

# Part V — Multiscale Storage, Bit/Nibble Packing, and Incrementality

## 29. Multiscale Processing Model

| **Ideal Mosaic rule** Preserve and address input as bytes; expose bits or nibbles only for genuine sub-byte meaning; form variable-length semantic tokens; process them in adaptive KiB blocks; organize them into MiB macroblocks for scale; and encode token IDs and metadata using bit-packed or entropy-coded representations outside the hot path. |
|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

## 30. Processing Blocks

| **Parameter**      | **Default target** | **Configurable range** | **Reason**                                      |
|--------------------|--------------------|------------------------|-------------------------------------------------|
| Minimum block      | 16 KiB             | 4-64 KiB               | Avoid excessive headers and scheduling overhead |
| Preferred block    | 64 KiB             | 16-512 KiB             | Good cache, edit, and parallelism balance       |
| Maximum block      | 256 KiB            | 64 KiB-4 MiB           | Bounds work when no safe boundary appears       |
| Overlap/lookbehind | Profile-dependent  | 0-64 KiB               | Preserve boundary and lexical context           |
| Macroblock         | 4 MiB              | 1-16 MiB               | Mapping, batching, integrity, distributed work  |

### 30.1 Boundary sources

- Paragraph and newline boundaries

- Unicode grapheme and language-pack boundaries

- Lexer restart positions and balanced structural boundaries

- Record delimiters in structured data

- Content-defined rolling-hash boundaries

- Model entropy or patch boundaries where applicable

- Application-supplied transaction or document boundaries

### 30.2 Chunking alternatives

| **Approach**                      | **Edit stability**               | **Semantic quality** | **Speed**                      | **Complexity**                 | **Recommended use**                        |
|-----------------------------------|----------------------------------|----------------------|--------------------------------|--------------------------------|--------------------------------------------|
| Fixed-size                        | Poor after insertion             | Poor at boundaries   | Highest                        | Lowest                         | Immutable batch corpora and simple storage |
| Pure content-defined              | High                             | Neutral              | High with FastCDC-like methods | Moderate                       | Cache/dedup dominated workloads            |
| Pure semantic                     | High when parser state is stable | Highest              | Variable                       | High; may produce uneven sizes | Compilers and structured documents         |
| Entropy/model-based               | Adaptive to information density  | Model-specific       | Requires model                 | Very high                      | LLM-native patching                        |
| Hybrid semantic + content-defined | High                             | High                 | Moderate to high               | Highest engineering effort     | Selected universal default                 |

FastCDC demonstrates that content-defined chunking can be substantially accelerated while preserving deduplication quality, reporting about 10x speed over open-source Rabin-based CDC and about 3x over then-state-of-the-art Gear/AE approaches \[R13\]. Mosaic-µ would not copy FastCDC blindly; it would combine content stability with semantic and restart evidence.

## 31. Incremental Processing

1\. Find the nearest safe preceding checkpoint.

2\. Invalidate source blocks intersecting the edit and any dependent continuation state.

3\. Retokenize forward under the same pack manifest.

4\. Compare terminal state, token hashes, and block boundary candidates with the prior version.

5\. Stop when a stable synchronization point is reached.

6\. Replace only affected Token IR ranges and emit consumer-specific edit events.

Tree-sitter provides practical evidence that incremental syntax processing can update structures on every editor keystroke \[R8\]. Mosaic-µ applies the same principle one layer earlier and exposes the affected regions to every consumer.

### 31.1 Estimated edit gains

| **Edit scenario**                 | **Tokenizer speedup vs full scan** | **Whole-application speedup** | **Caveat**                                        |
|-----------------------------------|------------------------------------|-------------------------------|---------------------------------------------------|
| Identifier or word replacement    | 50-500x                            | 5-50x                         | Assumes quick resynchronization                   |
| Local punctuation                 | 30-300x                            | 3-30x                         | May affect parser state locally                   |
| Whitespace or indentation         | 10-250x                            | 2-25x                         | Indentation-sensitive languages may expand region |
| Multiline string/comment          | 2-30x                              | 1.5-10x                       | State may propagate several blocks                |
| Delimiter removed near file start | 1-3x                               | 1-2x                          | Worst case can invalidate to EOF                  |

## 32. Content-Addressed Caching and Integrity

Each block cache key includes source content, tokenizer manifest, pack hashes, projection, and configuration. BLAKE3 is the recommended default because its official implementation describes an internal Merkle-tree design that is highly parallelizable and supports verified streaming and incremental updates \[R14\]. SHA-256 remains an option for compliance and interoperability.

> cache_key = HASH(  
> source_block \|\| predecessor_state_digest \|\|  
> tokenizer_manifest_hash \|\| pack_hashes \|\|  
> projection_id \|\| configuration_hash  
> )

### 32.1 Hash alternatives

| **Hash**      | **Speed**               | **Security**                     | **Incremental/tree use**             | **Compatibility**             | **Decision**                                   |
|---------------|-------------------------|----------------------------------|--------------------------------------|-------------------------------|------------------------------------------------|
| BLAKE3        | Very high and parallel  | Cryptographic                    | Native tree and streaming properties | Growing ecosystem             | Selected default                               |
| SHA-256       | Moderate                | Widely trusted and standardized  | Can be tree-wrapped externally       | Best compliance compatibility | Required optional profile                      |
| SHA-3         | Lower software speed    | Strong                           | External tree construction           | Standards use cases           | Optional                                       |
| xxHash/wyhash | Extremely high          | Non-cryptographic                | Good change detection only           | Broad                         | Use only for local non-security rolling checks |
| CRC32C        | Extremely high/hardware | Error detection, not adversarial | Good block corruption check          | Broad hardware support        | Optional secondary checksum                    |

## 33. Hot and Cold Token Representations

Hot processing should favor aligned u16 or u32 IDs and u32/u64 offsets. Cold storage and network transmission should use block-adaptive bit widths, delta-coded offsets, bitmaps, nibble-packed enums, local dictionaries, and optional entropy coding.

### 33.1 ID bit-width examples

| **Vocabulary size** | **Minimum fixed bits** | **Saving vs u32** |
|---------------------|------------------------|-------------------|
| 256                 | 8                      | 75.0%             |
| 4,096               | 12                     | 62.5%             |
| 32,768              | 15                     | 53.1%             |
| 65,536              | 16                     | 50.0%             |
| 128,000             | 17                     | 46.9%             |
| 262,144             | 18                     | 43.8%             |
| 1,048,576           | 20                     | 37.5%             |

### 33.2 Serialization alternatives

| **Representation**            | **Space**                      | **Decode speed**            | **Random access**    | **Editability** | **Decision**                    |
|-------------------------------|--------------------------------|-----------------------------|----------------------|-----------------|---------------------------------|
| u32 arrays                    | Worst                          | Best                        | Best                 | Best            | Hot memory default              |
| u16 arrays                    | 50% of u32 when possible       | Best                        | Best                 | Best            | Hot memory for \<=65K namespace |
| Tight fixed bit-packing       | 37-75% below u32               | Very high with block unpack | Block-level          | Moderate        | Cold default                    |
| Varint                        | Good for small/local IDs       | High but branchy            | Sequential/ indexed  | Moderate        | Offsets and sparse metadata     |
| Local dictionary + packed IDs | Excellent in repetitive blocks | High after dictionary load  | Block-level          | Moderate        | Optional per block              |
| Huffman                       | Entropy-aware                  | High                        | Poor without indexes | Poor            | Simple archive option           |
| rANS/tANS                     | Near-entropy compression       | Very high sequential decode | Poor                 | Poor            | Optional cold/archive transport |

### 33.3 Nibble packing

A nibble can encode 16 states. It is useful for token class codes, compression-method selectors, four-bit quantized metadata, BCD, and small block-local alphabets. Compared with a 32-bit enum, a 4-bit value saves 87.5% storage. Hot code may expand frequently accessed nibbles into bytes to avoid repeated masks and shifts.

## 34. Expected Multiscale Benefits

| **Metric**                               | **Estimated improvement vs whole-document byte stream**  |
|------------------------------------------|----------------------------------------------------------|
| **Large-file throughput**                | 1.5-6x with suitable parallelism and independent blocks  |
| **Peak active source memory**            | 90-99.98% lower                                          |
| **Ordinary local edit reprocessing**     | 98-99.99% lower bytes examined                           |
| **Versioned-document cache reuse**       | 30-99% depending on edit locality and repetition         |
| **Serialized token-ID size**             | 35-55% lower                                             |
| **Complete Token IR storage**            | 25-65% lower                                             |
| **Network transfer with entropy coding** | 35-75% lower                                             |
| **Partial random access**                | 10-1000x less data loaded                                |
| **Failure isolation**                    | Whole document reduced to one block or dependency region |

| **Small-input caveat** Below roughly 16-64 KiB, block selection, hashing, headers, and scheduling may produce no benefit or a 1-8% latency penalty. The runtime should bypass heavy multiscale machinery for small inputs. |
|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

# Part VI — Security, Reliability, APIs, and Implementation Language

## 35. Security Model

The tokenizer processes untrusted bytes, untrusted or third-party packs, malformed model files, and potentially adversarial Unicode. Security must therefore cover memory safety, resource exhaustion, protocol injection, Unicode ambiguity, pack authenticity, deterministic validation, and downstream mapping integrity.

### 35.1 Security requirements

- Bounds-check every pack offset, transition target, table width, and decoded count.

- Limit token length, lookahead, lookbehind, candidates per position, stack depth, nesting, normalization expansion, and VM steps.

- Reject cyclic structures where the schema requires acyclicity.

- Use deterministic automata instead of unbounded backtracking in the core.

- Isolate all unsafe Rust in reviewed low-level modules with documented invariants.

- Separate user bytes from typed control events.

- Preserve original bytes for audit even when normalized views are used.

- Support Unicode confusable, mixed-script, bidi-control, and invisible-character findings through optional security packs based on UTS \#39 \[R2\].

- Pin and verify pack hashes; optionally verify Ed25519 or policy-approved signatures.

- Fuzz input, packs, serialization, streaming chunk boundaries, and incremental edits.

### 35.2 Resource policy example

> ResourcePolicy {  
> max_source_bytes, max_token_bytes, max_candidates_per_position,  
> max_normalization_expansion_ratio, max_nesting_depth,  
> max_vm_steps_per_input_byte, max_block_bytes, max_graph_edges,  
> max_output_tokens, max_pack_mapped_bytes  
> }

## 36. Public API Layers

### 36.1 Simple API

> let tokenizer = Tokenizer::load("model.mtok")?;  
> let ids = tokenizer.encode(text.as_bytes())?;  
> let bytes = tokenizer.decode(&ids)?;

### 36.2 Structured API

> let result = session.process(Source::borrowed(input), ProcessOptions {  
> view: View::Lexical,  
> offsets: true,  
> structure: StructureLevel::IdentifiersAndNumbers,  
> normalization: Normalization::Preserve,  
> languages: LanguageSelection::Preferred(&\["en", "hi", "mr"\]),  
> ..Default::default()  
> })?;

### 36.3 Incremental API

> let update = document.apply_edit(Edit {  
> byte_range: 1024..1036,  
> replacement: b"new_identifier",  
> })?;  
> for event in update.consumer_events() { /\* reparse/reindex/update model \*/ }

### 36.4 C ABI

> mosaic_status mosaic_document_create(  
> const uint8_t\* input, size_t input_len,  
> const mosaic_options\* options,  
> mosaic_document\*\* out_document);  
>   
> mosaic_status mosaic_projection_ids(  
> const mosaic_document\* document,  
> const char\* model_projection,  
> mosaic_u32_view\* out_ids);

## 37. Programming Language Selection

Stable Rust is the recommended reference implementation language. Rust supports a no_std core that links to the platform-agnostic core library rather than the standard library \[R10\]. Its foreign-function interface supports the C ABI, which is the practical stable cross-language boundary \[R11\]\[R12\]. The design benefits from borrow-checked source spans, explicit ownership, safe concurrency, deterministic layouts through explicit repr(C) boundaries, and the ability to isolate unsafe SIMD and memory-map operations.

### 37.1 Language comparison

| **Language** | **Runtime size**             | **Memory safety**                  | **Peak performance**           | **Interop**          | **Development risk**                 | **Estimated project effect**                                                              | **Decision**                                    |
|--------------|------------------------------|------------------------------------|--------------------------------|----------------------|--------------------------------------|-------------------------------------------------------------------------------------------|-------------------------------------------------|
| Rust         | Small; no_std capable        | Strong by default                  | Near C/C++                     | Strong C ABI         | Moderate learning/toolchain cost     | 30-70% fewer memory-safety defects than manual C estimate; 0-10% performance cost or gain | Selected                                        |
| C            | Smallest potential           | Manual                             | Excellent                      | Universal            | Highest security and lifetime risk   | Possibly 5-20% smaller binary; 2-5x audit burden                                          | Fallback/independent conformance implementation |
| C++          | Moderate                     | Manual with tools                  | Excellent                      | C ABI wrapper needed | High language/ABI complexity         | Comparable speed; 20-50% larger maintenance surface estimate                              | Not primary                                     |
| Zig          | Small                        | Safer ergonomics but manual memory | Excellent                      | Excellent C interop  | Younger ecosystem and stability risk | Potential 0-10% size/speed gain; higher ecosystem risk                                    | Secondary candidate                             |
| Go           | Larger runtime               | Memory safe                        | Good, less predictable latency | Cgo cost             | GC and binary footprint              | 10-50% slower hot path estimate; much larger core                                         | Bindings/services only                          |
| Java/C#      | Large runtime                | Memory safe                        | Good JIT performance           | FFI overhead         | GC/runtime dependency                | Unsuitable for tiny universal core                                                        | Bindings only                                   |
| Python/JS    | Very large relative overhead | Managed                            | Poor for core                  | Easy wrapper         | Interpreter and allocation overhead  | 10-100x slower likely for low-level hot path                                              | Tooling and bindings only                       |

### 37.2 Rust crate decomposition

> mosaic-core no_std; source spans, automata, byte fallback  
> mosaic-alloc arenas, graphs, dynamic documents  
> mosaic-pack pack schema, loader, verifier, mmap  
> mosaic-unicode generated Unicode pack support  
> mosaic-language routing and composable pack interfaces  
> mosaic-lex DFA and scanner VM execution  
> mosaic-model BPE/Unigram/model projections  
> mosaic-store blocks, packing, hashes, caches  
> mosaic-ffi stable C ABI  
> mosaic-cli inspect, tokenize, validate, benchmark  
> mosaic-build Unicode/lexer/pack generators  
> mosaic-train vocabulary and language-pack training

### 37.3 Unsafe-code policy

- Unsafe code only in dedicated modules for SIMD, memory mapping, FFI, and validated zero-copy table access.

- Every unsafe block must document preconditions and invariants.

- Miri, sanitizers, fuzzers, and architecture-specific tests cover unsafe modules.

- Panic must not unwind across the C ABI; use status codes and explicit error objects.

- The no_std core must support panic=abort and caller-provided buffers.

## 38. Performance Engineering

- ASCII fast paths and SIMD UTF-8 validation/transcoding where available.

- Generated character classes and minimized automata.

- Fused validation, classification, boundary tracking, and candidate matching where semantics permit.

- Zero-copy source spans and caller-provided output buffers.

- Memory-mapped packs and lazy hot/cold section paging.

- Thread-local scratch buffers and arena reuse.

- Batch parallelism by processing block and document.

- Optional CPU dispatch for SSE4.2, AVX2, AVX-512, NEON, SVE, and RISC-V vectors.

- No rich graph construction in IDs-only mode.

simdutf demonstrates that Unicode validation and transcoding can operate at gigabytes per second using modern SIMD, with production use across major systems \[R15\]. Mosaic-µ should use this as a feasibility reference, not as a guarantee that its richer pipeline will equal a pure transcoder.

### 38.1 Runtime target sizes

| **Configuration**                       | **Binary/data target**           | **Typical working set** |
|-----------------------------------------|----------------------------------|-------------------------|
| Byte-only microkernel                   | 0.4-1 MiB                        | 0.2-1 MiB               |
| Unicode text                            | 2-6 MiB total                    | 1-4 MiB                 |
| One language + one 64K model            | 4-15 MiB                         | 3-10 MiB                |
| Multilingual desktop pack set           | 10-50 MiB installed              | 5-20 MiB resident       |
| Broad global pack catalog               | 100-500 MiB installed            | 10-40 MiB resident      |
| Many model packs and developer profiles | 250 MiB to several GiB installed | 20-100 MiB resident     |

## 39. Reliability and Error Handling

- Malformed source produces explicit error tokens or opaque byte spans rather than process failure.

- Malformed packs fail closed before execution.

- A pack incompatibility identifies the exact missing or conflicting dependency.

- Every projection records whether it is complete, partial, recovered, normalized, or synthetic.

- Applications may request strict mode, recovery mode, or best-effort mode.

- All integer arithmetic related to offsets and lengths is checked.

- No public API relies on locale-sensitive global process state.

# Part VII — Implementation Option Comparisons and Selection

This part compares multiple implementation solutions. Estimated improvements are relative and workload-dependent. They are intended for design selection and benchmark planning, not public performance claims.

## 40. End-to-End Architecture Alternatives

| **Architecture**                   | **Core size**         | **Extensibility** | **Whole-pipeline efficiency** | **Security**         | **Estimated outcome**                                                        | **Selection**           |
|------------------------------------|-----------------------|-------------------|-------------------------------|----------------------|------------------------------------------------------------------------------|-------------------------|
| Monolithic universal executable    | Large                 | Requires rebuild  | Good when everything is used  | Large attack surface | 10-30% faster startup in fully loaded appliance; 2-10x larger base footprint | Reject as default       |
| Independent per-domain tokenizers  | Small individually    | Fragmented        | Poor due to duplicate scans   | Inconsistent         | Simple adoption; 20-60% more combined memory/work in multi-consumer apps     | Compatibility mode only |
| Microkernel + declarative packs    | Small                 | High              | High with shared Token IR     | Strong               | 20-55% lower duplicate preprocessing; sub-MiB core target                    | Selected                |
| Service-only centralized tokenizer | Thin clients          | High centrally    | Network-dependent             | Centralized risk     | Good fleet consistency; added latency and availability dependency            | Optional deployment     |
| Model-integrated tokenizer only    | Tiny external runtime | Model-specific    | Excellent for LLM only        | Model surface        | 20-35% shorter global model sequences possible                               | Optional LLM branch     |

## 41. Normalization Alternatives

| **Policy**                                    | **Compression/search benefit** | **Exactness**          | **Security/audit**            | **Complexity**      | **Decision**                    |
|-----------------------------------------------|--------------------------------|------------------------|-------------------------------|---------------------|---------------------------------|
| No normalization anywhere                     | Lowest interoperability        | Exact                  | Preserves evidence            | Low                 | Available preserve-only profile |
| Destructive normalization before tokenization | High apparent consistency      | Source lost or altered | Can hide distinctions         | Low                 | Reject as canonical behavior    |
| Shadow normalized view with mappings          | High where requested           | Exact source retained  | Best auditability             | Moderate            | Selected                        |
| Consumer performs its own normalization       | Flexible                       | Variable               | Inconsistent across consumers | High duplicate work | Compatibility only              |

## 42. Pack Granularity Alternatives

| **Granularity**                            | **Installed size**        | **Reuse**                     | **Routing complexity**       | **Expected effect**                                      | **Decision**           |
|--------------------------------------------|---------------------------|-------------------------------|------------------------------|----------------------------------------------------------|------------------------|
| One global pack                            | Large                     | Poor selective loading        | Low                          | Fast simple deployment; multilingual imbalance likely    | Reject default         |
| One pack per language                      | Moderate to huge catalog  | Duplicates script/locale data | Medium                       | Easy mental model; 10-30% duplicate data estimate        | Not sufficient alone   |
| Script + language + locale + domain layers | Small reusable components | Highest                       | High                         | 20-50% lower duplicate pack data; better composition     | Selected               |
| Fully dynamic remote knowledge             | Minimal local             | Potentially high              | Very high; availability risk | Good thin clients; unacceptable deterministic dependency | Optional registry mode |

## 43. Token IR Alternatives

| **Representation**         | **Speed**                 | **Memory**   | **Expressiveness** | **Interoperability**    | **Decision**                 |
|----------------------------|---------------------------|--------------|--------------------|-------------------------|------------------------------|
| Flat IDs only              | Highest                   | Lowest       | Lowest             | Model-specific          | Fast projection only         |
| Flat tokens + offsets      | High                      | Low          | Moderate           | Good                    | Minimum general result       |
| Tree only                  | Moderate                  | Moderate     | Good hierarchy     | Overlap awkward         | Not sufficient               |
| Graph of heap objects      | Low                       | High         | Highest            | Poor binary stability   | Reject implementation        |
| Packed arrays + indexes    | High                      | Configurable | High               | Stable schemas possible | Selected                     |
| Columnar Arrow-like layout | High analytics throughput | Moderate     | Good batch use     | Strong data ecosystem   | Optional batch serialization |

## 44. Scanner VM Alternatives

| **Choice**             | **Pack expressiveness**            | **Runtime size**             | **Security**           | **Performance**   | **Decision**                       |
|------------------------|------------------------------------|------------------------------|------------------------|-------------------|------------------------------------|
| No VM; automata only   | Low to medium                      | Smallest                     | Best                   | Highest           | Tier-0/regular languages           |
| Tiny custom bounded VM | High enough for lexical edge cases | Small                        | Strong if verified     | High              | Selected optional extension        |
| WebAssembly            | Very high                          | Adds runtime                 | Good sandbox potential | Moderate to high  | Optional external advanced plugins |
| Lua/Python/JS plugin   | Very high                          | Large                        | Harder sandbox         | Low               | Reject core                        |
| Native callbacks       | Unlimited                          | Small runtime, external code | Worst                  | Highest potential | Privileged escape hatch only       |

## 45. Parallelization Alternatives

| **Approach**               | **Best case**                  | **Overhead**                   | **Correctness complexity** | **Use**                                   |
|----------------------------|--------------------------------|--------------------------------|----------------------------|-------------------------------------------|
| Single-thread fused scan   | 1x                             | Lowest                         | Lowest                     | Small and latency-sensitive inputs        |
| Document-level parallelism | Near-linear across documents   | Low                            | Low                        | Batch datasets and servers                |
| Block-level parallelism    | 2-10x expected on large inputs | Boundary and merge cost        | Medium                     | Large files and multi-consumer processing |
| GPU tokenization           | High batch potential           | Transfer and kernel setup      | High                       | Massive offline corpora only              |
| Hybrid CPU/GPU             | Potential highest throughput   | Highest engineering complexity | High                       | Model-integrated serving after profiling  |

## 46. Recommended Selection by Deployment

| **Deployment**                      | **Configuration**                                                                                                               |
|-------------------------------------|---------------------------------------------------------------------------------------------------------------------------------|
| **Embedded binary parser**          | no_std Rust core; byte source; DFA profile; no Unicode unless needed; fixed blocks; u16/u32 hot IDs; optional bit spans         |
| **Compiler/IDE**                    | Rust std + alloc; Unicode/script packs; lexer packs; rope/piece table source; semantic-content hybrid blocks; rich lexical IR   |
| **Multilingual NLP service**        | Unicode + selected script/language packs; explicit/preferred routing; constrained Unigram model projection; compact DAFSA packs |
| **Search/security platform**        | Rich Token IR; shadow normalization; security pack; language packs; memory-mapped source and columnar export                    |
| **LLM serving with existing model** | Model vocabulary pack; boundary-only language packs; IDs-only projection; aligned IDs; optional packed cache serialization      |
| **New LLM research**                | Byte-native local encoder; dynamic patches; typed controls; minimal external runtime; language packs as hints                   |
| **Large corpus preprocessing**      | Document and block parallelism; memory maps; compact packs expanded to fast tables; block hashes; packed cold output            |

# Part VIII — Forecast Metrics and Benchmark Methodology

## 47. Evidence Baselines

- SentencePiece reports BPE/Unigram support, about 50,000 sentences per second, and about a 6 MB memory footprint \[R5\].

- OpenAI tiktoken reports 3-6x speed over a specified older Hugging Face tokenizer baseline on 1 GB of text \[R6\].

- Hugging Face Tokenizers reports less than 20 seconds to tokenize 1 GB on a server CPU and supports alignment tracking \[R7\].

- Tree-sitter demonstrates incremental parsing designed to update on every editor keystroke \[R8\].

- BLT demonstrates dynamically sized entropy-based byte patches and byte-level scaling without a fixed vocabulary \[R16\].

- MEGABYTE demonstrates multiscale modeling of sequences exceeding one million bytes \[R17\].

- FastCDC demonstrates efficient content-defined chunking \[R13\].

| **Metric status** The following Mosaic values are forecasts derived from architecture and cited feasibility evidence. They are not comparable unless hardware, corpus, pack set, vocabulary, normalization, output density, and warm/cold state are controlled. |
|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

## 48. Universal Platform Headline Forecast

| **Area**                                   | **Conservative** | **Expected target** | **Strong result** |
|--------------------------------------------|------------------|---------------------|-------------------|
| English token reduction                    | 1%               | 3-6%                | 8%                |
| Broad multilingual token reduction         | 8%               | 12-22%              | 30%               |
| Poorly served languages with curated packs | 15%              | 20-35%              | 50%               |
| Mixed-language input                       | 8%               | 15-25%              | 35%               |
| Code token reduction                       | 6%               | 10-18%              | 25%               |
| Structured numbers/paths/identifiers       | 10%              | 18-30%              | 40%               |
| Whole-application preprocessing CPU        | 10% lower        | 20-55% lower        | 70% lower         |
| Temporary preprocessing memory             | 15% lower        | 25-50% lower        | 65% lower         |
| Incremental bytes reprocessed              | 90% lower        | 95-99.5% lower      | \>99.9% lower     |
| Incremental tokenizer latency              | 5x               | 20-300x             | 500x+             |
| Cross-language fertility variance          | 15% lower        | 25-55% lower        | 65% lower         |
| Minimal trusted runtime                    | \<1.5 MiB        | 0.4-1 MiB           | \<0.4 MiB         |

## 49. Raw Throughput Forecast

| **Mode**                         | **Relative to tiktoken** | **Relative to HF Tokenizers** | **CPU target** |
|----------------------------------|--------------------------|-------------------------------|----------------|
| Byte-only IDs                    | 1.1-2.5x                 | 2-5x                          | 1-5 GB/s       |
| Static vocabulary IDs            | 0.9-1.15x                | 1.3-2.5x                      | 200-800 MB/s   |
| Unicode + explicit language pack | 0.8-1.05x                | 1.2-2.2x                      | 150-600 MB/s   |
| Automatic multilingual routing   | 0.65-0.95x               | 1.0-1.8x                      | 100-450 MB/s   |
| Full lexical Token IR            | 0.6-0.9x                 | 0.9-1.7x                      | 100-400 MB/s   |
| IR + security metadata           | 0.45-0.8x                | 0.7-1.4x                      | 75-300 MB/s    |

The universal platform may be slower than flat BPE when measured only at encode-to-IDs. Its value appears in the end-to-end metric from input arrival to every required consumer being ready. Sharing Unicode analysis, offsets, language routing, lexical spans, and source buffers can produce a 15-50% whole-pipeline improvement even when the rich tokenizer pass itself is slower.

## 50. Language Pack Forecast

| **Input category**                      | **Fewer tokens vs strong general tokenizer** | **Effective context gain** | **Throughput gain** |
|-----------------------------------------|----------------------------------------------|----------------------------|---------------------|
| English                                 | 1-6%                                         | 1-6%                       | 0-7%                |
| Other high-resource Latin languages     | 6-15%                                        | 6-18%                      | 5-18%               |
| Arabic-script languages                 | 10-24%                                       | 11-32%                     | 10-30%              |
| Devanagari/Indic                        | 12-28%                                       | 14-39%                     | 12-35%              |
| Agglutinative languages                 | 12-27%                                       | 14-37%                     | 12-34%              |
| Chinese/Japanese                        | 8-22%                                        | 9-28%                      | 8-28%               |
| Thai/Lao/Khmer/Burmese                  | 12-30%                                       | 14-43%                     | 12-38%              |
| Low-resource language with curated pack | 15-35%                                       | 18-54%                     | 15-45%              |
| Code-switched text                      | 10-25%                                       | 11-33%                     | 10-32%              |

## 51. Context and Attention Scaling

| **Sequence reduction** | **Effective content increase at fixed token limit** | **Reduction in pairwise attention positions** |
|------------------------|-----------------------------------------------------|-----------------------------------------------|
| 5%                     | 5.3%                                                | 9.8%                                          |
| 10%                    | 11.1%                                               | 19.0%                                         |
| 15%                    | 17.6%                                               | 27.8%                                         |
| 20%                    | 25.0%                                               | 36.0%                                         |
| 25%                    | 33.3%                                               | 43.8%                                         |
| 30%                    | 42.9%                                               | 51.0%                                         |
| 35%                    | 53.8%                                               | 57.7%                                         |

## 52. Application-Pipeline Forecast

| **Application**                     | **Expected end-to-end improvement** | **Reason**                                                |
|-------------------------------------|-------------------------------------|-----------------------------------------------------------|
| Simple IDs-only application         | -5% to +10%                         | Rich architecture adds little when no structure is reused |
| Multilingual NLP service            | 15-35%                              | Language packs and shared Unicode/normalization           |
| Search/indexing                     | 15-40%                              | Shared terms, mappings, and zero-copy source              |
| Compiler front end                  | 5-25% full file                     | Fused Unicode/lexing and source maps                      |
| IDE interactive editing             | 2-20x lower update latency          | Incremental blocks and shared invalidation                |
| Formatter/refactoring               | 15-45%                              | Exact trivia and no repeated lexical scans                |
| Security-aware text pipeline        | 15-40%                              | Shared Unicode and source evidence                        |
| Mixed compiler + LLM developer tool | 20-50%                              | One source representation for multiple consumers          |

## 53. Storage Forecast

| **Metric**                                                    | **Estimated reduction** |
|---------------------------------------------------------------|-------------------------|
| Token IDs using fixed bit width                               | 35-55% vs u32           |
| Source offsets with delta/varint                              | 30-80%                  |
| Boolean metadata via bitmaps                                  | 75-97%                  |
| Repeated token classes                                        | 50-95%                  |
| Complete serialized Token IR                                  | 25-65%                  |
| Network/archive output with entropy coding                    | 35-75%                  |
| Combined memory when replacing several independent tokenizers | 20-60%                  |

## 54. Security and Exactness Targets

| **Metric**                                   | **Target** |
|----------------------------------------------|------------|
| **Arbitrary-byte representability**          | 100%       |
| **Exact source reconstruction**              | 100%       |
| **Unknown token rate**                       | 0          |
| **Unmapped non-synthetic derived tokens**    | 0          |
| **Plain-text control activation**            | 0          |
| **Native executable code in ordinary packs** | 0          |
| **Cross-platform deterministic vectors**     | 100% match |
| **Incremental/full equivalence**             | 100% match |
| **Streaming/full equivalence after EOF**     | 100% match |

## 55. Benchmark Matrix

- Corpora: English, major scripts, low-resource languages, code, mixed-language chat, structured data, logs, invalid UTF-8, binary formats, and adversarial inputs.

- Modes: cold start, warm pack, single thread, multi-thread, IDs only, offsets, lexical IR, full graph, explicit language, automatic routing, and incremental edits.

- Hardware: x86-64 AVX2, AVX-512 where available, ARM64 NEON, scalar fallback, embedded target, and server NUMA systems.

- Measures: bytes/s, tokens/s, allocations, resident memory, mapped bytes, block cache hit rate, fertility, token stability, edit locality, source-map correctness, and downstream model quality.

- Baselines: tiktoken, Hugging Face Tokenizers, SentencePiece, language-specific segmenters, compiler reference lexers, ICU boundaries, and model-native byte architectures where appropriate.

## 56. Public-Claim Gates

| **Claim type**                   | **Required evidence**                                                                        |
|----------------------------------|----------------------------------------------------------------------------------------------|
| **Faster than X**                | Same machine, input, vocabulary semantics, output density, thread count, and warm/cold state |
| **Fewer tokens**                 | Same model compatibility constraints and no hidden vocabulary-size inflation                 |
| **Better multilingual fairness** | Per-language distribution, worst-decile results, and corpus weighting disclosed              |
| **Smaller runtime**              | Separate executable, mapped pack data, resident working set, and dependency footprint        |
| **More secure**                  | Specific eliminated classes and adversarial tests; never claim vulnerability-free            |
| **Better LLM quality**           | Controlled training compute, data, model architecture, and multiple seeds                    |

# Part IX — Optional LLM-Only Branch

| **Scope status** This branch is optional and must not silently redefine the universal platform. It removes compiler, IDE, search, security, arbitrary-consumer, and rich Token IR requirements when a deployment is exclusively an LLM input/output system. |
|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

## 57. LLM-Only Static Architecture

> Exact bytes + typed controls  
> -\> Unicode/language boundary hints  
> -\> fixed model vocabulary projection  
> -\> token IDs and optional side channels  
> -\> embedding lookup and LLM

This mode is compatible with conventional Transformers. It retains byte fallback, language packs, typed controls, deterministic model IDs, and fast serving. It drops general consumer projections and most rich metadata.

### 57.1 Static LLM forecasts

| **Metric**                    | **Conservative** | **Expected**  | **Strong** |
|-------------------------------|------------------|---------------|------------|
| English token reduction       | 1%               | 3-7%          | 9%         |
| Multilingual token reduction  | 8%               | 12-25%        | 35%        |
| Low-resource packed languages | 15%              | 22-38%        | 50%        |
| Mixed-language input          | 10%              | 15-28%        | 38%        |
| Code                          | 7%               | 10-20%        | 27%        |
| KV-cache reduction            | 8%               | 12-25%        | 35%        |
| Long-context prefill latency  | 5% lower         | 10-30% lower  | 40% lower  |
| Text throughput               | 5% higher        | 10-35% higher | 45% higher |
| Tokenizer speed vs tiktoken   | 0.9x             | 1.0-1.15x     | 1.25x      |

## 58. Native Adaptive Byte-Patch Architecture

> Exact bytes + typed controls  
> -\> local byte encoder  
> -\> learned entropy/structure boundary predictor  
> -\> variable-length latent patches  
> -\> global language model  
> -\> local parallel-capable byte decoder  
> -\> exact output bytes

BLT provides direct evidence that dynamically sized entropy-based byte patches can match tokenized models at scale and improve inference efficiency and robustness \[R16\]. MEGABYTE demonstrates multiscale local/global byte modeling over million-byte sequences \[R17\]. A 2026 Fast BLT paper addresses the major byte-generation bottleneck through block-wise diffusion and speculative techniques \[R18\].

### 58.1 Native LLM forecasts

| **Metric**                       | **Conservative** | **Expected**   | **Strong**        |
|----------------------------------|------------------|----------------|-------------------|
| Global-model sequence reduction  | 15%              | 20-35%         | 45%               |
| Effective context increase       | 18%              | 25-54%         | 82%               |
| KV-cache reduction               | 15%              | 20-35%         | 45%               |
| Attention-pair reduction         | 28%              | 36-58%         | 70%               |
| Actual prefill latency reduction | 10%              | 15-40%         | 50%               |
| Global-model throughput          | 10% higher       | 20-50% higher  | 70% higher        |
| External CPU tokenization        | 50% lower        | 70-95% lower   | Nearly eliminated |
| Fixed vocabulary storage         | Eliminated       | Eliminated     | Eliminated        |
| Training complexity              | 10% higher       | 20-40% higher  | 60% higher        |
| Implementation complexity        | 30% higher       | 50-100% higher | \>2x              |

### 58.2 Decoder alternatives

| **Decoder**                     | **Generation speed vs token model** | **Quality/risk**                              | **Decision**                   |
|---------------------------------|-------------------------------------|-----------------------------------------------|--------------------------------|
| Autoregressive byte-by-byte     | 0.6-0.9x                            | Simple and exact; too many global/local steps | Research baseline only         |
| Local speculative decoding      | 0.9-1.3x                            | Verification overhead; practical path         | Recommended early optimization |
| Parallel block decoder          | 1.0-1.5x                            | Training and error-correction complexity      | Strong target                  |
| Diffusion + verification hybrid | 1.2-1.8x forecast                   | Most complex; quality must be proven          | Long-term experimental         |

## 59. Fixed Vocabulary versus Native Patches

| **Characteristic**            | **Static fixed vocabulary**        | **Native dynamic patches**                   |
|-------------------------------|------------------------------------|----------------------------------------------|
| Existing model compatibility  | High                               | None without retraining                      |
| Implementation risk           | Medium                             | Very high                                    |
| Tokenizer serving speed       | Excellent CPU                      | Patching inside model                        |
| New-language representability | Byte fallback                      | Immediate bytes                              |
| New-language efficiency       | Pack and often model adaptation    | Can improve through hints/continued training |
| Vocabulary embedding cost     | Potentially hundreds of MiB to GiB | Replaced by local modules                    |
| Generation maturity           | High                               | Emerging                                     |
| Near-term recommendation      | Primary production branch          | Research branch                              |
| Long-term ceiling             | High                               | Potentially highest                          |

## 60. LLM-Only Implementation Stack

| **Layer**                            | **Recommended technology**                              |
|--------------------------------------|---------------------------------------------------------|
| **Host byte runtime and protocol**   | Rust                                                    |
| **Training and model experiments**   | Python + PyTorch                                        |
| **Experimental accelerator kernels** | Triton                                                  |
| **Mature accelerator kernels**       | CUDA/C++ or vendor-neutral kernel backends where viable |
| **CPU inference kernels**            | Rust/C++ with SIMD                                      |
| **Deployment boundary**              | Stable C ABI plus framework bindings                    |

# Part X — Roadmap, Testing, Governance, Risks, and Final Decision

## 61. Development Roadmap

| **Version range** | **Milestone**                                | **Deliverables**                                                                               |
|-------------------|----------------------------------------------|------------------------------------------------------------------------------------------------|
| 0.0.1-0.0.3       | Byte substrate                               | Immutable/borrowed source, byte fallback, exact decode, source spans, basic C ABI, fuzzing.    |
| 0.0.4-0.0.6       | Pack foundation                              | Sectioned pack format, loader/verifier, DFA executor, command-line inspect/validate.           |
| 0.0.7-0.0.9       | Unicode tier                                 | Generated UTF-8, grapheme, script, and mapped normalization packs; conformance vectors.        |
| 0.1.0             | Minimal usable release                       | IDs-only byte/Unicode processing, stable manifest, deterministic round trip, Rust/C APIs.      |
| 0.2.x             | Boundary lattice and static model projection | Trie/DAFSA matching, Viterbi, BPE compatibility, constrained Unigram trainer.                  |
| 0.3.x             | Language pack system                         | Script/language/locale/domain dependencies, explicit/preferred routing, first reference packs. |
| 0.4.x             | Compiler profiles                            | DFA generator, scanner VM, error tokens, nested profiles, lexer conformance.                   |
| 0.5.x             | Token IR and processing runtime              | Views, mappings, payloads, capability negotiation, consumer cursors.                           |
| 0.6.x             | Incremental and multiscale                   | Blocks, macroblocks, hashes, cache, ropes/piece tables, edit events.                           |
| 0.7.x             | Security and registry                        | UTS \#39 pack, signatures, trust policies, resource budgets, pack registry.                    |
| 0.8.x             | Performance hardening                        | SIMD dispatch, fast table expansion, block parallelism, packed serialization.                  |
| 0.9.x             | Ecosystem validation                         | Bindings, import/export, large benchmark campaign, external pack authoring.                    |
| 1.0.0             | Stable universal standard                    | Freeze Token IR, pack ABI/schema, determinism, conformance suite, compatibility policy.        |
| 1.x research      | LLM static optimization                      | Model-vocabulary packs, multilingual balancing, adapters and benchmark models.                 |
| 2.x research      | Native byte patches                          | Local encoder/patcher/decoder reference architecture and training stack.                       |

## 62. Testing Strategy

| **Test class**           | **Examples**                                                       | **Pass criterion**                                 |
|--------------------------|--------------------------------------------------------------------|----------------------------------------------------|
| Property tests           | Round trip, complete coverage, source maps, idempotent views       | No counterexample across generated cases           |
| Fuzzing                  | Arbitrary bytes, malformed packs, serializers, VM, nested profiles | No crash, UB, unbounded work, or silent corruption |
| Unicode conformance      | UAX boundary tests, normalization tests, invalid sequences         | Match pinned Unicode version                       |
| Differential             | Full vs streaming, full vs incremental, scalar vs SIMD             | Bit-identical canonical output                     |
| Lexer conformance        | Reference compiler token streams and malformed partial code        | Declared compatibility target                      |
| Language quality         | Native-speaker corpora, dictionaries, mixed text                   | Pack-specific accuracy and fertility gates         |
| Performance              | Throughput, latency, allocations, cache, memory                    | Release targets on reference hardware              |
| Security                 | Confusables, controls, oversized fields, malicious packs           | Policy findings and fail-closed behavior           |
| Cross-language ABI       | C, Python, Java, JS, Go bindings                                   | Stable behavior and ownership correctness          |
| Long-running reliability | Streaming terabytes, repeated edits, pack churn                    | No leaks, drift, or state divergence               |

## 63. Pack Quality and Certification

| **Status**         | **Requirements**                                                                           |
|--------------------|--------------------------------------------------------------------------------------------|
| Experimental       | Builds, validates, and publishes data provenance                                           |
| Community verified | Independent tests and native-language review                                               |
| Conformant         | Passes mandatory format, exactness, determinism, and performance floors                    |
| Security audited   | Threat model, fuzzing, and independent review complete                                     |
| Reference          | Maintained under project governance with reproducible builds and compatibility commitments |

## 64. Governance Requirements

- Pack format and Token IR changes require versioned proposals and migration plans.

- Unicode version updates must not silently modify pinned results.

- Reference packs require documented corpus licenses and reproducible build manifests.

- Metric claims require benchmark artifacts and raw results.

- Security issues need a coordinated disclosure process and pack revocation mechanism.

- The core specification must remain implementation-neutral even when the reference runtime is Rust.

- Language communities should be able to own and improve packs without surrendering them to a centralized English-speaking team.

## 65. Risk Register

| **ID** | **Risk**                              | **Severity**                 | **Failure mode**                                                                                      | **Mitigation**                                                                      |
|--------|---------------------------------------|------------------------------|-------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------|
| R1     | Scope explosion                       | Critical                     | Attempting universal runtime, dozens of languages, compiler profiles, and LLM research simultaneously | Freeze phased scope; version 1 proves core and a few reference packs                |
| R2     | Pack ecosystem fragmentation          | High                         | Incompatible quality, licenses, or overlapping language packs                                         | Registry metadata, certification levels, deterministic dependency resolution        |
| R3     | No measurable English win             | Medium                       | Modern English tokenizers are already strong                                                          | Position value around multilingual, structure, processing reuse, and incrementality |
| R4     | Rich IR overhead exceeds savings      | High                         | Applications request unnecessary metadata or duplicate it anyway                                      | Capability negotiation, density profiles, benchmark end-to-end consumers            |
| R5     | Language detection errors             | Medium                       | Short/code-switched spans are ambiguous                                                               | Explicit metadata priority, confidence, fallback, no correctness dependence         |
| R6     | Vocabulary/model incompatibility      | Critical for pretrained LLMs | New tokens lack trained embeddings                                                                    | Boundary-only mode, adapter packs, retraining requirements clearly stated           |
| R7     | Scanner VM becomes unsafe complexity  | High                         | Feature creep makes it a general language                                                             | Non-Turing-complete ISA, verifier, budgets, strict instruction cap                  |
| R8     | Pack size undermines small-core story | Medium                       | Dictionaries and vocabularies are large                                                               | Report executable, mapped data, resident memory, and catalog size separately        |
| R9     | SIMD optimization causes divergence   | High                         | Architecture-specific bugs                                                                            | Scalar reference, differential testing, fuzzing, per-ISA CI                         |
| R10    | Hybrid chunking unstable              | Medium                       | Boundary algorithm changes cache identity                                                             | Version boundary policy and use content hashes independent of ordinal position      |
| R11    | Token IR standard gains no adoption   | High                         | Existing tools prefer their own formats                                                               | C ABI, conversion tools, incremental adoption, strong reference use cases           |
| R12    | Native byte LLM generation too slow   | Critical for LLM branch      | Byte-by-byte decoding bottleneck                                                                      | Keep static branch primary; pursue speculative/parallel decoder research            |
| R13    | Corpus bias in packs                  | High                         | Language packs encode harmful or narrow data distributions                                            | Data cards, source diversity, native review, fertility and robustness audits        |
| R14    | Security claims overreach             | High                         | Architecture is presented as vulnerability-free                                                       | Claim only eliminated classes; continuous fuzzing and disclosure                    |

## 66. Kill Criteria

| **Stop or redesign if** The core cannot remain below roughly 2 MiB without hiding mandatory dependencies; exact incremental and streaming equivalence cannot be maintained; language packs provide less than a 5% average benefit while adding material latency; IDs-only throughput remains below 70% of strong baselines; pack verification cannot bound execution; or the rich Token IR fails to reduce real multi-consumer processing cost. A project that ignores failed gates is no longer research. It is branding. |
|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

## 67. Recommended Initial Product

The first serious product should not be the entire universal vision. It should be a small, unusually correct tokenization runtime with a format and architecture that can grow.

- Rust no_std-capable core with borrowed and owned byte sources

- Exact byte fallback and source-span output

- Validated pack format and DFA executor

- Unicode 17 grapheme/script pack with mapped NFC view

- One constrained Unigram/BPE-compatible model pack

- English plus two structurally different reference language packs, preferably one Indic and one no-space segmentation language

- IDs-only, IDs+offsets, and lexical-span output densities

- Streaming/full equivalence, fuzzing, and a transparent benchmark harness

Only after this baseline proves exactness, pack modularity, and competitive speed should the project add rich Token IR consumers, language registry governance, hybrid content-defined blocks, or native byte-patch model research.

## 68. Final Architecture Decision

| **Final decision** Build Mosaic-µ as a Rust tokenization microkernel with exact byte source semantics, mapped Unicode views, a weighted boundary lattice, declarative and bounded packs, composable language specialization, packed-array Token IR, token-native consumer projections, adaptive KiB processing blocks, MiB macroblocks, and cold-path bit/nibble compression. Keep the LLM-only native byte-patch architecture as an explicitly separate research branch. |
|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|

## 69. Final Brutal Assessment

The project is technically credible because every essential mechanism has precedent: Unicode segmentation, generated DFA lexing, incremental parsing, subword tokenization, byte-native modeling, content-defined chunking, tree hashing, memory mapping, and bit-packed integer storage. The novelty is the disciplined integration and the decision to make exact source identity and consumer processing part of the tokenizer contract.

The project is commercially and organizationally difficult because its value grows with ecosystem adoption. A standalone tokenizer must compete with mature, simple libraries. Mosaic-µ becomes compelling only when one runtime replaces several inconsistent preprocessing stacks, or when language packs materially lower multilingual cost. The architecture should therefore target one concrete wedge: multilingual LLM preprocessing, compiler-plus-LLM developer tooling, or a high-assurance token-processing SDK. Trying to sell “universality” before proving one painful use case would be the traditional route to a beautiful repository with twelve stars.

# Appendix A — Representative Data Schemas

## A.1 Leaf Token

> LeafToken {  
> start_delta: varuint \| u32,  
> byte_length: varuint \| u32,  
> namespace: u16,  
> kind: u16,  
> flags: packed bits,  
> payload_ref: optional u32  
> }

## A.2 Structural Node

> StructuralNode {  
> source_spans: SpanRangeRef,  
> kind: TypeId,  
> parent: optional NodeId,  
> child_range: Range\<NodeId\>,  
> view_id: ViewId,  
> payload_ref: optional u32,  
> flags: u16  
> }

## A.3 Processing Block

> ProcessingBlock {  
> source_start, source_length,  
> predecessor_state_digest, terminal_state_digest,  
> block_hash, manifest_hash,  
> leaf_range, structure_range, view_ranges\[\],  
> packed_storage_descriptor, cache_status  
> }

## A.4 Transformation Record

> TransformationRecord {  
> transform_id, transform_version, parameters_hash,  
> input_view, output_view, mapping_ref,  
> loss_class: { LOSSLESS, SOURCE_PRESERVING_LOSSY_VIEW, SYNTHETIC },  
> pack_hash  
> }

# Appendix B — Example Pack Manifest

> pack_id = "org.mosaic.language.hi"  
> version = "1.0.0"  
> pack_type = "language"  
> languages = \["hi"\]  
> scripts = \["Devanagari"\]  
> unicode = "\>=17.0,\<18.0"  
> dependencies = \[  
> "org.mosaic.unicode.core@17.0",  
> "org.mosaic.script.devanagari@1"  
> \]  
> quality_level = 4  
> max_lookahead = 64  
> max_candidates_per_position = 32  
> content_hash = "blake3:..."  
> signature = "ed25519:..."  
> build_manifest = "reproducible-build-id"

# Appendix C — Requirement Traceability Summary

| **Requirement group**  | **Primary implementation**                      | **Primary verification**             |
|------------------------|-------------------------------------------------|--------------------------------------|
| Exactness and coverage | Byte source + canonical leaves                  | Arbitrary-byte property tests        |
| Unicode correctness    | Generated Unicode packs                         | Unicode conformance vectors          |
| Language neutrality    | Pack-free fallback + composable packs           | Remove packs and compare round trip  |
| Determinism            | Pinned manifests and integer algorithms         | Cross-platform golden outputs        |
| Incrementality         | Checkpoints, blocks, state digests              | Full/incremental differential tests  |
| Security               | Rust, pack verifier, bounded VM, typed controls | Fuzzing and malicious pack corpus    |
| Small core             | Offline generation and feature gating           | Stripped binary and dependency audit |
| Performance            | SIMD, automata, zero-copy, fast layouts         | Controlled benchmark matrix          |
| Storage                | Packed blocks and delta metadata                | Compression/decode benchmarks        |
| Consumer processing    | Token IR and projection runtime                 | Multi-consumer end-to-end workloads  |

# Appendix D — Reference Sources

**\[R1\]** Unicode Consortium. Unicode Standard Annex \#29: Unicode Text Segmentation, Unicode 17.0.0, Revision 47. https://www.unicode.org/reports/tr29/

**\[R2\]** Unicode Consortium. Unicode Technical Standard \#39: Unicode Security Mechanisms, Unicode 17.0.0. https://www.unicode.org/reports/tr39/

**\[R3\]** Unicode ICU. Boundary Analysis User Guide. https://unicode-org.github.io/icu/userguide/boundaryanalysis/

**\[R4\]** Unicode CLDR. Language Support Levels and Coverage Levels. https://cldr.unicode.org/index/language-support-levels

**\[R5\]** Google. SentencePiece repository and technical highlights. https://github.com/google/sentencepiece

**\[R6\]** OpenAI. tiktoken repository and published comparison benchmark. https://github.com/openai/tiktoken

**\[R7\]** Hugging Face. Tokenizers documentation. https://huggingface.co/docs/tokenizers/main/index

**\[R8\]** Tree-sitter. Introduction and lexical analysis documentation. https://tree-sitter.github.io/tree-sitter/

**\[R9\]** re2c 4.1 documentation. UTF-8-capable lexer generation and supported targets. https://re2c.org/

**\[R10\]** Rust Embedded Book. A no_std Rust Environment. https://doc.rust-lang.org/stable/embedded-book/intro/no-std.html

**\[R11\]** The Rust Reference. External blocks and the C ABI. https://doc.rust-lang.org/reference/items/external-blocks.html

**\[R12\]** Rust Embedded Book. C and Rust interoperability guidance. https://doc.rust-lang.org/stable/embedded-book/interoperability/

**\[R13\]** Xia et al. FastCDC: A Fast and Efficient Content-Defined Chunking Approach for Data Deduplication. USENIX ATC 2016. https://www.usenix.org/conference/atc16/technical-sessions/presentation/xia

**\[R14\]** BLAKE3 Team. Official BLAKE3 implementation and design summary. https://github.com/BLAKE3-team/BLAKE3

**\[R15\]** simdutf. Unicode validation and transcoding at GB/s. https://simdutf.github.io/simdutf/

**\[R16\]** Pagnoni et al. Byte Latent Transformer: Patches Scale Better Than Tokens. ACL 2025. https://aclanthology.org/2025.acl-long.453/

**\[R17\]** Yu et al. MEGABYTE: Predicting Million-byte Sequences with Multiscale Transformers. NeurIPS 2023. https://papers.neurips.cc/paper_files/paper/2023/hash/f8f78f8043f35890181a824e53a57134-Abstract-Conference.html

**\[R18\]** Kallini et al. Fast Byte Latent Transformer. arXiv:2605.08044, 2026. https://arxiv.org/abs/2605.08044

# Appendix E — Forecast Disclaimer

No Mosaic-µ benchmark exists at the time of this document. Forecast ranges are architectural estimates intended to define engineering targets and selection criteria. They must not be represented as observed performance. The appropriate next artifact after this specification is a benchmark and validation plan tied to a minimal prototype, not another layer of increasingly precise imaginary percentages.
