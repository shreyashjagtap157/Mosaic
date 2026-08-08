#ifndef MOSAIC_H
#define MOSAIC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOSAIC_C_API_VERSION_MAJOR 0
#define MOSAIC_C_API_VERSION_MINOR 11
#define MOSAIC_C_API_VERSION_PATCH 0

typedef enum mosaic_status {
    MOSAIC_OK = 0,
    MOSAIC_ERROR_INVALID_ARGUMENT = 1,
    MOSAIC_ERROR_IO = 2,
    MOSAIC_ERROR_INVALID_PACK = 3,
    MOSAIC_ERROR_OUT_OF_MEMORY = 4,
    MOSAIC_ERROR_OVERFLOW = 5,
    MOSAIC_ERROR_UNKNOWN_TOKEN_ID = 6,
    MOSAIC_ERROR_INTERNAL = 7,
    MOSAIC_ERROR_CONFLICT = 8,
    MOSAIC_ERROR_RESOURCE_LIMIT = 9,
    MOSAIC_ERROR_UNSUPPORTED = 10
} mosaic_status;

typedef struct mosaic_model mosaic_model;
typedef struct mosaic_unicode mosaic_unicode;
typedef struct mosaic_tokenizer mosaic_tokenizer;
typedef struct mosaic_detector mosaic_detector;
typedef struct mosaic_security mosaic_security;
typedef struct mosaic_normalization mosaic_normalization;
typedef struct mosaic_stream mosaic_stream;
typedef struct mosaic_online_stream mosaic_online_stream;
typedef struct mosaic_document mosaic_document;
typedef struct mosaic_incremental_document mosaic_incremental_document;
typedef struct mosaic_resync_document mosaic_resync_document;
typedef struct mosaic_token_document mosaic_token_document;

typedef struct mosaic_token {
    uint32_t id;
    uint64_t start;
    uint64_t length;
    int32_t cost;
} mosaic_token;

typedef struct mosaic_document_token {
    uint32_t id;
    uint64_t start;
    uint64_t length;
} mosaic_document_token;

enum {
    MOSAIC_TOKEN_DOCUMENT_MODEL = 1u << 0,
    MOSAIC_TOKEN_DOCUMENT_GRAPHEMES = 1u << 1,
    MOSAIC_TOKEN_DOCUMENT_SECURITY = 1u << 2,
    MOSAIC_TOKEN_DOCUMENT_NORMALIZATION = 1u << 3
};

enum {
    MOSAIC_CAP_MODEL = 1ull << 0,
    MOSAIC_CAP_GRAPHEMES = 1ull << 1,
    MOSAIC_CAP_LANGUAGE_PACKS = 1ull << 2,
    MOSAIC_CAP_DETECTOR = 1ull << 3,
    MOSAIC_CAP_SECURITY = 1ull << 4,
    MOSAIC_CAP_NORMALIZATION = 1ull << 5,
    MOSAIC_CAP_STREAMING = 1ull << 6,
    MOSAIC_CAP_ONLINE_STREAMING = 1ull << 7,
    MOSAIC_CAP_EDITABLE_DOCUMENT = 1ull << 8,
    MOSAIC_CAP_INCREMENTAL_DOCUMENT = 1ull << 9,
    MOSAIC_CAP_RESYNC_DOCUMENT = 1ull << 10,
    MOSAIC_CAP_TOKEN_DOCUMENT = 1ull << 11
};

typedef struct mosaic_range {
    uint64_t start;
    uint64_t length;
} mosaic_range;


typedef struct mosaic_script_span {
    uint64_t start;
    uint64_t length;
    uint16_t script_id;
    uint16_t reserved;
} mosaic_script_span;

typedef enum mosaic_security_kind {
    MOSAIC_SECURITY_BIDI_CONTROL = 1,
    MOSAIC_SECURITY_DEFAULT_IGNORABLE = 2,
    MOSAIC_SECURITY_NONCHARACTER = 3,
    MOSAIC_SECURITY_DEPRECATED = 4,
    MOSAIC_SECURITY_MIXED_SCRIPT = 5
} mosaic_security_kind;

typedef struct mosaic_security_finding {
    uint32_t kind;
    uint16_t script_id;
    uint16_t reserved;
    uint64_t start;
    uint64_t length;
} mosaic_security_finding;

typedef mosaic_status (*mosaic_security_visitor)(void *context, const mosaic_security_finding *finding);


typedef enum mosaic_normalization_mode {
    MOSAIC_NORMALIZE_PRESERVE = 0,
    MOSAIC_NORMALIZE_NFD = 1,
    MOSAIC_NORMALIZE_NFC = 2,
    MOSAIC_NORMALIZE_NFKD = 3,
    MOSAIC_NORMALIZE_NFKC = 4,
    MOSAIC_NORMALIZE_NFKC_CASEFOLD = 5
} mosaic_normalization_mode;

typedef struct mosaic_normalized_unit {
    uint64_t output_start;
    uint64_t output_length;
    uint32_t source_span_index;
    uint32_t source_span_count;
} mosaic_normalized_unit;

typedef struct mosaic_normalized_view {
    uint8_t *bytes;
    size_t byte_length;
    mosaic_normalized_unit *units;
    size_t unit_count;
    mosaic_range *source_spans;
    size_t source_span_count;
} mosaic_normalized_view;

typedef struct mosaic_detection {
    uint32_t matched;
    uint32_t available;
    int64_t score;
    int64_t margin;
    char language[64];
} mosaic_detection;

typedef struct mosaic_token_document_options {
    uint32_t struct_size;
    uint32_t flags;
    mosaic_normalization_mode normalization_mode;
    uint32_t reserved;
} mosaic_token_document_options;

typedef struct mosaic_tokenizer_capabilities {
    uint32_t struct_size;
    uint32_t reserved;
    uint64_t available;
} mosaic_tokenizer_capabilities;

typedef struct mosaic_token_document_info {
    uint32_t flags;
    uint64_t source_length;
    uint64_t model_token_count;
    uint64_t grapheme_count;
    uint64_t security_finding_count;
    uint64_t normalized_byte_length;
    uint64_t normalized_unit_count;
    mosaic_normalization_mode normalization_mode;
    uint32_t reserved;
    uint8_t source_sha256[32];
    uint8_t tokenizer_fingerprint_sha256[32];
    mosaic_detection detection;
} mosaic_token_document_info;

/* Returned buffers are owned by Mosaic and released with mosaic_free(). */
void mosaic_free(void *pointer);
const char *mosaic_version_string(void);
/* Canonical tokenization-semantics version. This changes only when identical packs/input may
 * have different canonical interpretation under the runtime. It is intentionally independent
 * from the release and C ABI versions. */
uint32_t mosaic_tokenizer_semantics_version(void);
const char *mosaic_status_string(mosaic_status status);


/* High-level integrated tokenizer. Both pack byte sequences are copied and validated. */
mosaic_status mosaic_tokenizer_load_memory(const uint8_t *model_pack, size_t model_pack_len,
                                           const uint8_t *unicode_pack, size_t unicode_pack_len,
                                           mosaic_tokenizer **out_tokenizer);
mosaic_status mosaic_tokenizer_load_files(const char *model_path, const char *unicode_path,
                                          mosaic_tokenizer **out_tokenizer);
/* Optional external language-specialization packs are copied, validated, and owned by the tokenizer.
 * At most one pack for a given BCP47-style language tag may be loaded in v0.3. */
mosaic_status mosaic_tokenizer_add_language_memory(mosaic_tokenizer *tokenizer,
                                                   const uint8_t *language_pack, size_t language_pack_len);
mosaic_status mosaic_tokenizer_add_language_file(mosaic_tokenizer *tokenizer, const char *path);
size_t mosaic_tokenizer_language_count(const mosaic_tokenizer *tokenizer);
mosaic_status mosaic_tokenizer_language_tag(const mosaic_tokenizer *tokenizer, size_t index,
                                            char *buffer, size_t capacity, size_t *out_required);
/* Optional document-level detector pack. A low-confidence or unavailable result falls back
 * to the base model and never affects exact representability. One detector may be attached in v0.3. */
mosaic_status mosaic_tokenizer_set_detector_memory(mosaic_tokenizer *tokenizer,
                                                   const uint8_t *detector_pack, size_t detector_pack_len);
mosaic_status mosaic_tokenizer_set_detector_file(mosaic_tokenizer *tokenizer, const char *path);
/* Optional Unicode-17 security/script evidence pack. Findings are evidence, never source rewriting. */
mosaic_status mosaic_tokenizer_set_security_memory(mosaic_tokenizer *tokenizer,
                                                   const uint8_t *security_pack, size_t security_pack_len);
mosaic_status mosaic_tokenizer_set_security_file(mosaic_tokenizer *tokenizer, const char *path);
int mosaic_tokenizer_security_loaded(const mosaic_tokenizer *tokenizer);
mosaic_status mosaic_tokenizer_security_scan(const mosaic_tokenizer *tokenizer,
                                              const uint8_t *input, size_t input_len,
                                              mosaic_security_finding **out_findings, size_t *out_count);
mosaic_status mosaic_tokenizer_security_visit(const mosaic_tokenizer *tokenizer,
                                               const uint8_t *input, size_t input_len,
                                               mosaic_security_visitor visitor, void *context, size_t *out_count);
/* Optional mapped-normalization pack. The pack version is independent of segmentation/security packs. */
mosaic_status mosaic_tokenizer_set_normalization_memory(mosaic_tokenizer *tokenizer,
                                                        const uint8_t *normalization_pack, size_t normalization_pack_len);
mosaic_status mosaic_tokenizer_set_normalization_file(mosaic_tokenizer *tokenizer, const char *path);
int mosaic_tokenizer_normalization_loaded(const mosaic_tokenizer *tokenizer);
mosaic_status mosaic_tokenizer_normalize(const mosaic_tokenizer *tokenizer, mosaic_normalization_mode mode,
                                         const uint8_t *input, size_t input_len, mosaic_normalized_view *out_view);
int mosaic_tokenizer_detector_loaded(const mosaic_tokenizer *tokenizer);
mosaic_status mosaic_tokenizer_detect_language(const mosaic_tokenizer *tokenizer,
                                               const uint8_t *input, size_t input_len,
                                               mosaic_detection *out_detection);
mosaic_status mosaic_tokenizer_encode_auto(const mosaic_tokenizer *tokenizer,
                                           const uint8_t *input, size_t input_len,
                                           uint32_t **out_ids, size_t *out_count,
                                           mosaic_detection *out_detection);
mosaic_status mosaic_tokenizer_encode_tokens_auto(const mosaic_tokenizer *tokenizer,
                                                  const uint8_t *input, size_t input_len,
                                                  mosaic_token **out_tokens, size_t *out_count,
                                                  mosaic_detection *out_detection);
void mosaic_tokenizer_free(mosaic_tokenizer *tokenizer);
/* Stable SHA-256 fingerprint of semantic runtime version + exact loaded pack bytes. */
mosaic_status mosaic_tokenizer_fingerprint(const mosaic_tokenizer *tokenizer, uint8_t out_sha256[32]);
mosaic_status mosaic_tokenizer_get_capabilities(const mosaic_tokenizer *tokenizer, mosaic_tokenizer_capabilities *out_capabilities);
mosaic_status mosaic_tokenizer_encode(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t input_len,
                                      uint32_t **out_ids, size_t *out_count);
mosaic_status mosaic_tokenizer_encode_tokens(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t input_len,
                                             mosaic_token **out_tokens, size_t *out_count);
mosaic_status mosaic_tokenizer_decode(const mosaic_tokenizer *tokenizer, const uint32_t *ids, size_t count,
                                      uint8_t **out_bytes, size_t *out_len);
mosaic_status mosaic_tokenizer_grapheme_ranges(const mosaic_tokenizer *tokenizer,
                                               const uint8_t *input, size_t input_len,
                                               mosaic_range **out_ranges, size_t *out_count);
mosaic_status mosaic_tokenizer_stream_create(const mosaic_tokenizer *tokenizer, mosaic_stream **out_stream);
/* Auto-routing stream snapshots model/Unicode/language/detector configuration at creation. */
mosaic_status mosaic_tokenizer_stream_create_auto(const mosaic_tokenizer *tokenizer, mosaic_stream **out_stream);
mosaic_status mosaic_tokenizer_document_create(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t input_len,
                                               mosaic_document **out_document);
/* Auto-routing document re-detects from the current exact bytes after each edit. */
mosaic_status mosaic_tokenizer_document_create_auto(const mosaic_tokenizer *tokenizer, const uint8_t *input, size_t input_len,
                                                    mosaic_document **out_document);


/* Immutable Token IR snapshot. Source bytes are authoritative; projections use byte coordinates. */
mosaic_status mosaic_tokenizer_token_document_create(const mosaic_tokenizer *tokenizer,
                                                       const uint8_t *input, size_t input_len, uint32_t flags,
                                                       mosaic_token_document **out_document);
mosaic_status mosaic_tokenizer_token_document_create_auto(const mosaic_tokenizer *tokenizer,
                                                            const uint8_t *input, size_t input_len, uint32_t flags,
                                                            mosaic_token_document **out_document);
mosaic_status mosaic_tokenizer_token_document_create_ex(const mosaic_tokenizer *tokenizer,
                                                          const uint8_t *input, size_t input_len,
                                                          const mosaic_token_document_options *options,
                                                          mosaic_token_document **out_document);
mosaic_status mosaic_tokenizer_token_document_create_auto_ex(const mosaic_tokenizer *tokenizer,
                                                               const uint8_t *input, size_t input_len,
                                                               const mosaic_token_document_options *options,
                                                               mosaic_token_document **out_document);
mosaic_status mosaic_token_document_get_info(const mosaic_token_document *document, mosaic_token_document_info *out_info);
mosaic_status mosaic_token_document_copy_source(const mosaic_token_document *document, uint8_t **out_bytes, size_t *out_len);
mosaic_status mosaic_token_document_model_tokens(const mosaic_token_document *document,
                                                  mosaic_document_token **out_tokens, size_t *out_count);
mosaic_status mosaic_token_document_graphemes(const mosaic_token_document *document,
                                               mosaic_range **out_ranges, size_t *out_count);
mosaic_status mosaic_token_document_security_findings(const mosaic_token_document *document,
                                                       mosaic_security_finding **out_findings, size_t *out_count);
mosaic_status mosaic_token_document_normalized_view(const mosaic_token_document *document,
                                                     mosaic_normalized_view *out_view);
void mosaic_token_document_free(mosaic_token_document *document);

/* Pack bytes are copied; caller may release its input immediately. */
mosaic_status mosaic_model_load_memory(const uint8_t *pack, size_t pack_len, mosaic_model **out_model);
mosaic_status mosaic_model_load_file(const char *path, mosaic_model **out_model);
void mosaic_model_free(mosaic_model *model);

/* Exact arbitrary-byte tokenization. */
mosaic_status mosaic_encode(const mosaic_model *model, const uint8_t *input, size_t input_len,
                            uint32_t **out_ids, size_t *out_count);
mosaic_status mosaic_encode_tokens(const mosaic_model *model, const uint8_t *input, size_t input_len,
                                   mosaic_token **out_tokens, size_t *out_count);
mosaic_status mosaic_decode(const mosaic_model *model, const uint32_t *ids, size_t count,
                            uint8_t **out_bytes, size_t *out_len);

/* Streaming v0.1 is semantically exact and buffers until EOF. The stream owns
 * an internal copy of the model, so the source model may be released after creation. */
mosaic_status mosaic_stream_create(const mosaic_model *model, mosaic_stream **out_stream);
mosaic_status mosaic_stream_push(mosaic_stream *stream, const uint8_t *bytes, size_t len);
mosaic_status mosaic_stream_finish(mosaic_stream *stream, uint32_t **out_ids, size_t *out_count);
/* Valid for auto-routing streams; returns the document-level detection used at EOF. */
mosaic_status mosaic_stream_finish_auto(mosaic_stream *stream, uint32_t **out_ids, size_t *out_count,
                                        mosaic_detection *out_detection);
mosaic_status mosaic_stream_reset(mosaic_stream *stream);
void mosaic_stream_free(mosaic_stream *stream);

/* Exact online Viterbi stream. Unlike mosaic_stream, this API commits token prefixes before EOF
 * and bounds unresolved source bytes by max_pending_bytes. Raw-BPE models are intentionally
 * unsupported because their merge semantics require a different online compatibility algorithm.
 * On MOSAIC_ERROR_RESOURCE_LIMIT, out_consumed reports how many input bytes were accepted and
 * out_ids may contain prefixes already proven canonical. */
mosaic_status mosaic_online_stream_create(const mosaic_model *model, size_t max_pending_bytes,
                                          mosaic_online_stream **out_stream);
mosaic_status mosaic_tokenizer_online_stream_create(const mosaic_tokenizer *tokenizer, size_t max_pending_bytes,
                                                     mosaic_online_stream **out_stream);
mosaic_status mosaic_online_stream_push(mosaic_online_stream *stream, const uint8_t *bytes, size_t len,
                                        size_t *out_consumed, uint32_t **out_ids, size_t *out_count);
mosaic_status mosaic_online_stream_finish(mosaic_online_stream *stream, uint32_t **out_ids, size_t *out_count);
size_t mosaic_online_stream_pending_bytes(const mosaic_online_stream *stream);
void mosaic_online_stream_free(mosaic_online_stream *stream);

/* Editable-document v0.1 preserves exact semantics and currently retokenizes the
 * complete document after edits. Future incremental engines must be equivalent. */
mosaic_status mosaic_document_create(const mosaic_model *model, const uint8_t *input, size_t input_len,
                                     mosaic_document **out_document);
mosaic_status mosaic_document_apply_edit(mosaic_document *document, uint64_t start, uint64_t delete_len,
                                         const uint8_t *replacement, size_t replacement_len);
mosaic_status mosaic_document_encode(const mosaic_document *document, uint32_t **out_ids, size_t *out_count);
/* Valid for auto-routing documents; detection reflects the current edited bytes. */
mosaic_status mosaic_document_encode_auto(const mosaic_document *document, uint32_t **out_ids, size_t *out_count,
                                          mosaic_detection *out_detection);
mosaic_status mosaic_document_copy_bytes(const mosaic_document *document, uint8_t **out_bytes, size_t *out_len);
void mosaic_document_free(mosaic_document *document);

/* Exact Viterbi incremental document. The implementation preserves an unchanged canonical prefix
 * and retokenizes only from a proven-safe token boundary preceding an edit. Raw-BPE models are
 * explicitly unsupported by this API. The operation is transactional: a failed edit leaves the
 * prior document and token cache unchanged. */
mosaic_status mosaic_incremental_document_create(const mosaic_model *model, const uint8_t *input, size_t input_len,
                                                 mosaic_incremental_document **out_document);
mosaic_status mosaic_tokenizer_incremental_document_create(const mosaic_tokenizer *tokenizer,
                                                           const uint8_t *input, size_t input_len,
                                                           mosaic_incremental_document **out_document);
mosaic_status mosaic_incremental_document_apply_edit(mosaic_incremental_document *document,
                                                      uint64_t start, uint64_t delete_len,
                                                      const uint8_t *replacement, size_t replacement_len);
mosaic_status mosaic_incremental_document_encode(const mosaic_incremental_document *document,
                                                  uint32_t **out_ids, size_t *out_count);
mosaic_status mosaic_incremental_document_copy_bytes(const mosaic_incremental_document *document,
                                                      uint8_t **out_bytes, size_t *out_len);
size_t mosaic_incremental_document_last_reprocessed_bytes(const mosaic_incremental_document *document);
size_t mosaic_incremental_document_last_reused_prefix_bytes(const mosaic_incremental_document *document);
void mosaic_incremental_document_free(mosaic_incremental_document *document);

/* Checkpoint-resynchronizing exact Viterbi document. Checkpoints snapshot the bounded online
 * survivor state. After an edit, matching a shifted old checkpoint proves future execution state
 * identity and permits exact suffix reuse without scanning to EOF. */
mosaic_status mosaic_resync_document_create(const mosaic_model *model, const uint8_t *input, size_t input_len,
                                            size_t checkpoint_bytes, size_t max_pending_bytes,
                                            mosaic_resync_document **out_document);
mosaic_status mosaic_tokenizer_resync_document_create(const mosaic_tokenizer *tokenizer,
                                                      const uint8_t *input, size_t input_len,
                                                      size_t checkpoint_bytes, size_t max_pending_bytes,
                                                      mosaic_resync_document **out_document);
mosaic_status mosaic_resync_document_apply_edit(mosaic_resync_document *document,
                                                 uint64_t start, uint64_t delete_len,
                                                 const uint8_t *replacement, size_t replacement_len);
mosaic_status mosaic_resync_document_encode(const mosaic_resync_document *document,
                                             uint32_t **out_ids, size_t *out_count);
mosaic_status mosaic_resync_document_copy_bytes(const mosaic_resync_document *document,
                                                 uint8_t **out_bytes, size_t *out_len);
size_t mosaic_resync_document_last_reprocessed_bytes(const mosaic_resync_document *document);
size_t mosaic_resync_document_last_reused_prefix_bytes(const mosaic_resync_document *document);
size_t mosaic_resync_document_last_reused_suffix_bytes(const mosaic_resync_document *document);
int mosaic_resync_document_last_resynchronized(const mosaic_resync_document *document);
void mosaic_resync_document_free(mosaic_resync_document *document);

mosaic_status mosaic_detector_load_memory(const uint8_t *pack, size_t pack_len, mosaic_detector **out_detector);
mosaic_status mosaic_detector_load_file(const char *path, mosaic_detector **out_detector);
void mosaic_detector_free(mosaic_detector *detector);
mosaic_status mosaic_detector_detect(const mosaic_detector *detector, const uint8_t *input, size_t input_len,
                                     mosaic_detection *out_detection);

mosaic_status mosaic_unicode_load_memory(const uint8_t *pack, size_t pack_len, mosaic_unicode **out_unicode);
mosaic_status mosaic_unicode_load_file(const char *path, mosaic_unicode **out_unicode);
void mosaic_unicode_free(mosaic_unicode *unicode_data);

/* Script/security pack APIs. The pack is independent of the Unicode segmentation pack. */
mosaic_status mosaic_security_load_memory(const uint8_t *pack, size_t pack_len, mosaic_security **out_security);
mosaic_status mosaic_security_load_file(const char *path, mosaic_security **out_security);
void mosaic_security_free(mosaic_security *security);
mosaic_status mosaic_security_script_ranges(const mosaic_security *security, const uint8_t *input, size_t input_len,
                                            mosaic_script_span **out_ranges, size_t *out_count);
mosaic_status mosaic_security_script_name(const mosaic_security *security, uint16_t script_id,
                                          char *buffer, size_t capacity, size_t *out_required);
mosaic_status mosaic_security_scan(const mosaic_security *security, const uint8_t *input, size_t input_len,
                                   mosaic_security_finding **out_findings, size_t *out_count);
mosaic_status mosaic_security_visit(const mosaic_security *security, const uint8_t *input, size_t input_len,
                                    mosaic_security_visitor visitor, void *context, size_t *out_count);

/* Version-pinned normalization shadow views. Source bytes are never modified. */
mosaic_status mosaic_normalization_load_memory(const uint8_t *pack, size_t pack_len, mosaic_normalization **out_normalization);
mosaic_status mosaic_normalization_load_file(const char *path, mosaic_normalization **out_normalization);
void mosaic_normalization_free(mosaic_normalization *normalization);
mosaic_status mosaic_normalization_unicode_version(const mosaic_normalization *normalization,
                                                    uint16_t *major, uint16_t *minor, uint16_t *micro);
mosaic_status mosaic_normalize(const mosaic_normalization *normalization, mosaic_normalization_mode mode,
                               const uint8_t *input, size_t input_len, mosaic_normalized_view *out_view);
void mosaic_normalized_view_free(mosaic_normalized_view *view);

/* Invalid UTF-8 bytes are returned as one-byte opaque grapheme ranges. */
mosaic_status mosaic_grapheme_ranges(const mosaic_unicode *unicode_data,
                                     const uint8_t *input, size_t input_len,
                                     mosaic_range **out_ranges, size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif
