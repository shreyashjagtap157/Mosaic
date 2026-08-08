#include "mosaic.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#define BATCH 200
#define SEQ_MAX 1024
#define OP_MAX 8

typedef struct observer_state {
    mtx_t mutex;
    uint8_t fingerprint[32];
    unsigned char seen[SEQ_MAX];
    atomic_uint_fast64_t success;
    atomic_uint_fast64_t failure;
    atomic_uint_fast64_t resource;
    atomic_uint_fast64_t bad;
    atomic_uint_fast64_t success_by_operation[OP_MAX + 1];
    atomic_uint_fast64_t failure_by_operation[OP_MAX + 1];
    atomic_uint_fast64_t resource_by_operation[OP_MAX + 1];
    atomic_int reentered;
    mosaic_tokenizer *tokenizer;
} observer_state;

static int fail(const char *message) { fprintf(stderr, "%s\n", message); return 1; }

static void observe(void *context, const mosaic_event *event) {
    observer_state *state = (observer_state *)context;
    if (!event || event->struct_size != sizeof *event ||
        memcmp(event->tokenizer_fingerprint_sha256, state->fingerprint, 32u) != 0 ||
        !event->sequence || event->sequence >= SEQ_MAX || event->operation < 1u || event->operation > OP_MAX) {
        atomic_fetch_add_explicit(&state->bad, 1u, memory_order_relaxed);
        return;
    }
    if (mtx_lock(&state->mutex) != thrd_success) {
        atomic_fetch_add_explicit(&state->bad, 1u, memory_order_relaxed);
        return;
    }
    if (state->seen[event->sequence]) atomic_fetch_add_explicit(&state->bad, 1u, memory_order_relaxed);
    state->seen[event->sequence] = 1u;
    mtx_unlock(&state->mutex);

    if (event->kind == MOSAIC_EVENT_SUCCESS && event->status == MOSAIC_OK) {
        atomic_fetch_add_explicit(&state->success, 1u, memory_order_relaxed);
        atomic_fetch_add_explicit(&state->success_by_operation[event->operation], 1u, memory_order_relaxed);
        /* Re-enter the same tokenizer exactly once. The recursion guard must suppress nested observer delivery. */
        if (event->operation == MOSAIC_OPERATION_ENCODE && state->tokenizer &&
            !atomic_exchange_explicit(&state->reentered, 1, memory_order_relaxed)) {
            const uint8_t nested[] = "x";
            uint32_t *ids = NULL; size_t count = 0;
            if (mosaic_tokenizer_encode(state->tokenizer, nested, 1u, &ids, &count) != MOSAIC_OK || !count)
                atomic_fetch_add_explicit(&state->bad, 1u, memory_order_relaxed);
            mosaic_free(ids);
        }
    } else if (event->kind == MOSAIC_EVENT_RESOURCE_REJECTED && event->status == MOSAIC_ERROR_RESOURCE_LIMIT && event->resource_limit) {
        atomic_fetch_add_explicit(&state->resource, 1u, memory_order_relaxed);
        atomic_fetch_add_explicit(&state->resource_by_operation[event->operation], 1u, memory_order_relaxed);
    } else if (event->kind == MOSAIC_EVENT_FAILURE && event->status != MOSAIC_OK) {
        atomic_fetch_add_explicit(&state->failure, 1u, memory_order_relaxed);
        atomic_fetch_add_explicit(&state->failure_by_operation[event->operation], 1u, memory_order_relaxed);
    } else {
        atomic_fetch_add_explicit(&state->bad, 1u, memory_order_relaxed);
    }
}

static uint64_t load_counter(const atomic_uint_fast64_t *counter) {
    return atomic_load_explicit(counter, memory_order_relaxed);
}

int main(int argc, char **argv) {
    if (argc != 8) return fail("usage: observability_smoke MODEL UNICODE DETECTOR EN SECURITY NORMALIZATION LEXER");
    mosaic_tokenizer *tokenizer = NULL;
    if (mosaic_tokenizer_load_files(argv[1], argv[2], &tokenizer) != MOSAIC_OK ||
        mosaic_tokenizer_add_language_file(tokenizer, argv[4]) != MOSAIC_OK ||
        mosaic_tokenizer_set_detector_file(tokenizer, argv[3]) != MOSAIC_OK ||
        mosaic_tokenizer_set_security_file(tokenizer, argv[5]) != MOSAIC_OK ||
        mosaic_tokenizer_set_normalization_file(tokenizer, argv[6]) != MOSAIC_OK ||
        mosaic_tokenizer_set_lexer_file(tokenizer, argv[7]) != MOSAIC_OK)
        return fail("load integrated tokenizer");

    observer_state state = {0};
    state.tokenizer = tokenizer;
    if (mtx_init(&state.mutex, mtx_plain) != thrd_success) return fail("observer mutex");
    if (mosaic_tokenizer_fingerprint(tokenizer, state.fingerprint) != MOSAIC_OK) return fail("fingerprint");
    uint8_t identity_before[32], identity_after[32];
    if (mosaic_tokenizer_runtime_identity(tokenizer, identity_before) != MOSAIC_OK) return fail("identity before");
    mosaic_observer_config observer = {sizeof observer, 0u, MOSAIC_OBSERVE_SUCCESS | MOSAIC_OBSERVE_FAILURE | MOSAIC_OBSERVE_RESOURCE, 0u, observe, &state};
    if (mosaic_tokenizer_set_observer(tokenizer, &observer) != MOSAIC_OK ||
        mosaic_tokenizer_runtime_identity(tokenizer, identity_after) != MOSAIC_OK || memcmp(identity_before, identity_after, 32u) != 0)
        return fail("observer changed runtime identity");
    mosaic_observer_config readback = {0};
    if (mosaic_tokenizer_get_observer(tokenizer, &readback) != MOSAIC_OK || readback.callback != observe || readback.context != &state)
        return fail("observer readback");

    mosaic_runtime_limits limits; mosaic_runtime_limits_default(&limits); limits.max_input_bytes = 16u;
    if (mosaic_tokenizer_set_runtime_limits(tokenizer, &limits) != MOSAIC_OK || mosaic_tokenizer_seal(tokenizer) != MOSAIC_OK)
        return fail("seal");
    if (mosaic_tokenizer_set_observer(tokenizer, &observer) != MOSAIC_ERROR_STATE) return fail("observer mutation after seal");

    const uint8_t hello[] = "hello"; uint32_t *ids = NULL; size_t count = 0;
    if (mosaic_tokenizer_encode(tokenizer, hello, 5u, &ids, &count) != MOSAIC_OK || !count) return fail("success encode");
    mosaic_free(ids);
    const uint8_t over[] = "12345678901234567";
    if (mosaic_tokenizer_encode(tokenizer, over, sizeof over - 1u, &ids, &count) != MOSAIC_ERROR_RESOURCE_LIMIT) return fail("resource encode");
    const uint32_t invalid = 0xffffffffu; uint8_t *decoded = NULL; size_t decoded_len = 0;
    if (mosaic_tokenizer_decode(tokenizer, &invalid, 1u, &decoded, &decoded_len) != MOSAIC_ERROR_UNKNOWN_TOKEN_ID) return fail("failure decode");

    const uint8_t detector_text[] = "tokenizer"; mosaic_detection detection = {0};
    if (mosaic_tokenizer_detect_language(tokenizer, detector_text, sizeof detector_text - 1u, &detection) != MOSAIC_OK ||
        !detection.matched || strcmp(detection.language, "en")) return fail("detect event");
    mosaic_range *ranges = NULL; size_t range_count = 0;
    if (mosaic_tokenizer_grapheme_ranges(tokenizer, hello, 5u, &ranges, &range_count) != MOSAIC_OK || !range_count) return fail("grapheme event");
    mosaic_free(ranges);
    mosaic_security_finding *findings = NULL; size_t finding_count = 0;
    if (mosaic_tokenizer_security_scan(tokenizer, hello, 5u, &findings, &finding_count) != MOSAIC_OK) return fail("security event");
    mosaic_free(findings);
    const uint8_t decomposed[] = {'e', 0xcc, 0x81}; mosaic_normalized_view normalized = {0};
    if (mosaic_tokenizer_normalize(tokenizer, MOSAIC_NORMALIZE_NFC, decomposed, sizeof decomposed, &normalized) != MOSAIC_OK || normalized.byte_length != 2u)
        return fail("normalize event");
    mosaic_normalized_view_free(&normalized);
    const uint8_t code[] = "int x=1;"; mosaic_lex_token *lex = NULL; size_t lex_count = 0;
    if (mosaic_tokenizer_lex(tokenizer, code, sizeof code - 1u, &lex, &lex_count) != MOSAIC_OK || !lex_count) return fail("lex event");
    mosaic_free(lex);

    mosaic_token_document_options options = {sizeof options,
        MOSAIC_TOKEN_DOCUMENT_MODEL | MOSAIC_TOKEN_DOCUMENT_GRAPHEMES | MOSAIC_TOKEN_DOCUMENT_SECURITY |
        MOSAIC_TOKEN_DOCUMENT_NORMALIZATION | MOSAIC_TOKEN_DOCUMENT_LEXICAL | MOSAIC_TOKEN_DOCUMENT_SEMANTIC,
        MOSAIC_NORMALIZE_NFC, 0u};
    mosaic_token_document *document = NULL;
    if (mosaic_tokenizer_token_document_create_ex(tokenizer, code, sizeof code - 1u, &options, &document) != MOSAIC_OK || !document)
        return fail("token document event");
    mosaic_token_document_free(document);

    mosaic_executor_config ec; mosaic_executor_config_default(&ec); ec.worker_count = 4u; ec.queue_capacity = 4u; ec.max_batch_items = BATCH; ec.max_total_input_bytes = BATCH * 5u;
    mosaic_executor *executor = NULL;
    if (mosaic_executor_create(&ec, &executor) != MOSAIC_OK) return fail("executor");
    mosaic_batch_input inputs[BATCH];
    for (size_t i = 0; i < BATCH; ++i) inputs[i] = (mosaic_batch_input){hello, 5u};
    mosaic_batch_result *results = NULL;
    if (mosaic_executor_encode_batch(executor, tokenizer, inputs, BATCH, &results) != MOSAIC_OK) return fail("parallel observed batch");
    for (size_t i = 0; i < BATCH; ++i) if (results[i].status != MOSAIC_OK) return fail("batch item");
    mosaic_batch_results_free(results, BATCH); mosaic_executor_free(executor);

    const uint64_t success = load_counter(&state.success), failure = load_counter(&state.failure),
                   resource = load_counter(&state.resource), bad = load_counter(&state.bad);
    if (success != BATCH + 12u || failure != 1u || resource != 1u || bad != 0u) {
        fprintf(stderr, "observer counters success=%llu failure=%llu resource=%llu bad=%llu\n",
                (unsigned long long)success, (unsigned long long)failure, (unsigned long long)resource, (unsigned long long)bad);
        return fail("observer counters");
    }
    if (load_counter(&state.success_by_operation[MOSAIC_OPERATION_ENCODE]) != BATCH + 2u ||
        load_counter(&state.success_by_operation[MOSAIC_OPERATION_DETECT]) != 1u ||
        load_counter(&state.success_by_operation[MOSAIC_OPERATION_GRAPHEMES]) != 2u ||
        load_counter(&state.success_by_operation[MOSAIC_OPERATION_SECURITY]) != 2u ||
        load_counter(&state.success_by_operation[MOSAIC_OPERATION_NORMALIZE]) != 2u ||
        load_counter(&state.success_by_operation[MOSAIC_OPERATION_LEX]) != 2u ||
        load_counter(&state.success_by_operation[MOSAIC_OPERATION_TOKEN_DOCUMENT]) != 1u ||
        load_counter(&state.failure_by_operation[MOSAIC_OPERATION_DECODE]) != 1u ||
        load_counter(&state.resource_by_operation[MOSAIC_OPERATION_ENCODE]) != 1u)
        return fail("operation-specific observer counters");
    for (size_t i = 1; i <= BATCH + 14u; ++i) if (!state.seen[i]) return fail("observer sequence gap");
    if (!atomic_load_explicit(&state.reentered, memory_order_relaxed)) return fail("observer reentrancy guard not exercised");

    mosaic_tokenizer_capabilities caps = {sizeof caps, 0u, 0u};
    if (mosaic_tokenizer_get_capabilities(tokenizer, &caps) != MOSAIC_OK || !(caps.available & MOSAIC_CAP_OBSERVABILITY))
        return fail("observability capability");
    printf("OK observer success=%llu failure=%llu resource=%llu concurrent=%u operations=8\n",
           (unsigned long long)success, (unsigned long long)failure, (unsigned long long)resource, BATCH);
    mosaic_tokenizer_free(tokenizer); mtx_destroy(&state.mutex);
    return 0;
}
