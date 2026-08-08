#ifndef MOSAIC_H
#define MOSAIC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOSAIC_C_API_VERSION_MAJOR 0
#define MOSAIC_C_API_VERSION_MINOR 4
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
    MOSAIC_ERROR_CONFLICT = 8
} mosaic_status;

typedef struct mosaic_model mosaic_model;
typedef struct mosaic_unicode mosaic_unicode;
typedef struct mosaic_tokenizer mosaic_tokenizer;
typedef struct mosaic_detector mosaic_detector;
typedef struct mosaic_stream mosaic_stream;
typedef struct mosaic_document mosaic_document;

typedef struct mosaic_token {
    uint32_t id;
    uint64_t start;
    uint64_t length;
    int32_t cost;
} mosaic_token;

typedef struct mosaic_range {
    uint64_t start;
    uint64_t length;
} mosaic_range;

typedef struct mosaic_detection {
    uint32_t matched;
    uint32_t available;
    int64_t score;
    int64_t margin;
    char language[64];
} mosaic_detection;

/* Returned buffers are owned by Mosaic and released with mosaic_free(). */
void mosaic_free(void *pointer);
const char *mosaic_version_string(void);
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

mosaic_status mosaic_detector_load_memory(const uint8_t *pack, size_t pack_len, mosaic_detector **out_detector);
mosaic_status mosaic_detector_load_file(const char *path, mosaic_detector **out_detector);
void mosaic_detector_free(mosaic_detector *detector);
mosaic_status mosaic_detector_detect(const mosaic_detector *detector, const uint8_t *input, size_t input_len,
                                     mosaic_detection *out_detection);

mosaic_status mosaic_unicode_load_memory(const uint8_t *pack, size_t pack_len, mosaic_unicode **out_unicode);
mosaic_status mosaic_unicode_load_file(const char *path, mosaic_unicode **out_unicode);
void mosaic_unicode_free(mosaic_unicode *unicode_data);

/* Invalid UTF-8 bytes are returned as one-byte opaque grapheme ranges. */
mosaic_status mosaic_grapheme_ranges(const mosaic_unicode *unicode_data,
                                     const uint8_t *input, size_t input_len,
                                     mosaic_range **out_ranges, size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif
