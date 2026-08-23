#include "mosaic.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "mosaic_thread.h"

#define MOSAIC_EXECUTOR_MAX_WORKERS 256u
#define MOSAIC_EXECUTOR_MAX_QUEUE 1048576u

typedef struct mosaic_batch_context mosaic_batch_context;

typedef struct mosaic_executor_task {
    const mosaic_tokenizer *tokenizer;
    const uint8_t *input;
    size_t input_len;
    mosaic_batch_result *result;
    mosaic_batch_context *batch;
} mosaic_executor_task;

struct mosaic_batch_context {
    mosaic_mutex_t mutex;
    mosaic_cond_t done;
    size_t remaining;
};

struct mosaic_executor {
    mosaic_executor_config config;
    mosaic_thread_t *workers;
    mosaic_executor_task *queue;
    size_t head;
    size_t tail;
    size_t queued;
    int stopping;
    mosaic_mutex_t mutex;
    mosaic_cond_t not_empty;
    mosaic_cond_t not_full;
    atomic_uint_fast64_t batches;
    atomic_uint_fast64_t items;
    atomic_uint_fast64_t succeeded_items;
    atomic_uint_fast64_t failed_items;
    atomic_uint_fast64_t input_bytes;
};

static void batch_complete(mosaic_batch_context *batch) {
    if (!mosaic_mutex_lock(&batch->mutex)) return;
    if (batch->remaining) --batch->remaining;
    if (!batch->remaining) mosaic_cond_broadcast(&batch->done);
    mosaic_mutex_unlock(&batch->mutex);
}

static int executor_worker(void *context) {
    mosaic_executor *executor = (mosaic_executor *)context;
    for (;;) {
        if (!mosaic_mutex_lock(&executor->mutex)) return -1;
        while (!executor->stopping && !executor->queued) {
            if (!mosaic_cond_wait(&executor->not_empty, &executor->mutex)) {
                mosaic_mutex_unlock(&executor->mutex);
                return -1;
            }
        }
        if (executor->stopping && !executor->queued) {
            mosaic_mutex_unlock(&executor->mutex);
            return 0;
        }
        mosaic_executor_task task = executor->queue[executor->head];
        executor->head = (executor->head + 1u) % executor->config.queue_capacity;
        --executor->queued;
        mosaic_cond_signal(&executor->not_full);
        mosaic_mutex_unlock(&executor->mutex);

        uint32_t *ids = NULL;
        size_t count = 0;
        mosaic_status status = mosaic_tokenizer_encode(task.tokenizer, task.input, task.input_len, &ids, &count);
        task.result->status = status;
        task.result->ids = ids;
        task.result->count = status == MOSAIC_OK ? count : 0u;
        if (status != MOSAIC_OK) { mosaic_free(ids); task.result->ids = NULL; }
        atomic_fetch_add_explicit(&executor->items, 1u, memory_order_relaxed);
        atomic_fetch_add_explicit(&executor->input_bytes, (uint64_t)task.input_len, memory_order_relaxed);
        if (status == MOSAIC_OK) atomic_fetch_add_explicit(&executor->succeeded_items, 1u, memory_order_relaxed);
        else atomic_fetch_add_explicit(&executor->failed_items, 1u, memory_order_relaxed);
        batch_complete(task.batch);
    }
}

void mosaic_executor_config_default(mosaic_executor_config *out_config) {
    if (!out_config) return;
    *out_config = (mosaic_executor_config){sizeof *out_config, 0u, 4u, 1024u, 65536u, 1024ull * 1024ull * 1024ull};
}

void mosaic_executor_config_low_memory_default(mosaic_executor_config *out_config) {
    if (!out_config) return;
    *out_config = (mosaic_executor_config){sizeof *out_config, 0u, 1u, 64u, 4096u, 64ull * 1024ull * 1024ull};
}

mosaic_status mosaic_executor_create(const mosaic_executor_config *config, mosaic_executor **out_executor) {
    if (!config || !out_executor || config->struct_size < sizeof *config || config->flags ||
        !config->worker_count || config->worker_count > MOSAIC_EXECUTOR_MAX_WORKERS ||
        !config->queue_capacity || config->queue_capacity > MOSAIC_EXECUTOR_MAX_QUEUE ||
        !config->max_batch_items || !config->max_total_input_bytes) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_executor = NULL;
    mosaic_executor *executor = (mosaic_executor *)calloc(1u, sizeof *executor);
    if (!executor) return MOSAIC_ERROR_OUT_OF_MEMORY;
    executor->config = *config;
    executor->workers = (mosaic_thread_t *)calloc(config->worker_count, sizeof *executor->workers);
    executor->queue = (mosaic_executor_task *)calloc(config->queue_capacity, sizeof *executor->queue);
    if (!executor->workers || !executor->queue) goto oom;
    if (!mosaic_mutex_init(&executor->mutex)) goto init_fail;
    int mutex_ready = 1;
    if (!mosaic_cond_init(&executor->not_empty)) goto cnd_empty_fail;
    int empty_ready = 1;
    if (!mosaic_cond_init(&executor->not_full)) goto cnd_full_fail;
    size_t created = 0;
    for (; created < config->worker_count; ++created) {
        if (!mosaic_thread_create(&executor->workers[created], executor_worker, executor)) break;
    }
    if (created != config->worker_count) {
        if (mosaic_mutex_lock(&executor->mutex)) {
            executor->stopping = 1;
            mosaic_cond_broadcast(&executor->not_empty);
            mosaic_mutex_unlock(&executor->mutex);
        }
        for (size_t i = 0; i < created; ++i) mosaic_thread_join(executor->workers[i]);
        mosaic_cond_destroy(&executor->not_full);
        mosaic_cond_destroy(&executor->not_empty);
        mosaic_mutex_destroy(&executor->mutex);
        free(executor->queue); free(executor->workers); free(executor);
        return MOSAIC_ERROR_INTERNAL;
    }
    *out_executor = executor;
    return MOSAIC_OK;

cnd_full_fail:
    if (empty_ready) mosaic_cond_destroy(&executor->not_empty);
cnd_empty_fail:
    if (mutex_ready) mosaic_mutex_destroy(&executor->mutex);
init_fail:
    free(executor->queue); free(executor->workers); free(executor);
    return MOSAIC_ERROR_INTERNAL;
oom:
    free(executor->queue); free(executor->workers); free(executor);
    return MOSAIC_ERROR_OUT_OF_MEMORY;
}

mosaic_status mosaic_executor_encode_batch(mosaic_executor *executor, const mosaic_tokenizer *tokenizer,
                                           const mosaic_batch_input *inputs, size_t input_count,
                                           mosaic_batch_result **out_results) {
    if (!executor || !tokenizer || (!inputs && input_count) || !out_results) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_results = NULL;
    if (!mosaic_tokenizer_is_sealed(tokenizer)) return MOSAIC_ERROR_STATE;
    if ((uint64_t)input_count > executor->config.max_batch_items) return MOSAIC_ERROR_RESOURCE_LIMIT;
    if (input_count > SIZE_MAX / sizeof(mosaic_batch_result)) return MOSAIC_ERROR_OVERFLOW;
    uint64_t total = 0;
    for (size_t i = 0; i < input_count; ++i) {
        if (!inputs[i].data && inputs[i].length) return MOSAIC_ERROR_INVALID_ARGUMENT;
        if ((uint64_t)inputs[i].length > executor->config.max_total_input_bytes - total) return MOSAIC_ERROR_RESOURCE_LIMIT;
        total += (uint64_t)inputs[i].length;
    }
    if (!input_count) return MOSAIC_OK;
    mosaic_batch_result *results = (mosaic_batch_result *)calloc(input_count, sizeof *results);
    if (!results) return MOSAIC_ERROR_OUT_OF_MEMORY;
    mosaic_batch_context batch = {0};
    if (!mosaic_mutex_init(&batch.mutex)) { free(results); return MOSAIC_ERROR_INTERNAL; }
    if (!mosaic_cond_init(&batch.done)) { mosaic_mutex_destroy(&batch.mutex); free(results); return MOSAIC_ERROR_INTERNAL; }
    batch.remaining = 0u;

    for (size_t i = 0; i < input_count; ++i) {
        if (!mosaic_mutex_lock(&batch.mutex)) goto submit_fail;
        ++batch.remaining;
        mosaic_mutex_unlock(&batch.mutex);
        if (!mosaic_mutex_lock(&executor->mutex)) { batch_complete(&batch); goto submit_fail; }
        while (!executor->stopping && executor->queued == executor->config.queue_capacity) {
            if (!mosaic_cond_wait(&executor->not_full, &executor->mutex)) {
                mosaic_mutex_unlock(&executor->mutex);
                batch_complete(&batch);
                goto submit_fail;
            }
        }
        if (executor->stopping) { mosaic_mutex_unlock(&executor->mutex); batch_complete(&batch); goto submit_fail; }
        executor->queue[executor->tail] = (mosaic_executor_task){tokenizer, inputs[i].data, inputs[i].length, &results[i], &batch};
        executor->tail = (executor->tail + 1u) % executor->config.queue_capacity;
        ++executor->queued;
        mosaic_cond_signal(&executor->not_empty);
        mosaic_mutex_unlock(&executor->mutex);
    }

    if (!mosaic_mutex_lock(&batch.mutex)) goto wait_fail;
    while (batch.remaining) {
        if (!mosaic_cond_wait(&batch.done, &batch.mutex)) {
            mosaic_mutex_unlock(&batch.mutex);
            goto wait_fail;
        }
    }
    mosaic_mutex_unlock(&batch.mutex);
    mosaic_cond_destroy(&batch.done); mosaic_mutex_destroy(&batch.mutex);
    atomic_fetch_add_explicit(&executor->batches, 1u, memory_order_relaxed);
    *out_results = results;
    return MOSAIC_OK;

submit_fail:
    /* Wait for already-submitted items so their batch-context pointer cannot outlive this stack frame. */
    if (mosaic_mutex_lock(&batch.mutex)) {
        while (batch.remaining) {
            if (!mosaic_cond_wait(&batch.done, &batch.mutex)) break;
        }
        mosaic_mutex_unlock(&batch.mutex);
    }
wait_fail:
    mosaic_cond_destroy(&batch.done); mosaic_mutex_destroy(&batch.mutex);
    mosaic_batch_results_free(results, input_count);
    return MOSAIC_ERROR_INTERNAL;
}

void mosaic_batch_results_free(mosaic_batch_result *results, size_t count) {
    if (!results) return;
    for (size_t i = 0; i < count; ++i) mosaic_free(results[i].ids);
    free(results);
}

mosaic_status mosaic_executor_get_metrics(const mosaic_executor *executor, mosaic_executor_metrics *out_metrics) {
    if (!executor || !out_metrics) return MOSAIC_ERROR_INVALID_ARGUMENT;
    *out_metrics = (mosaic_executor_metrics){
        atomic_load_explicit(&executor->batches, memory_order_relaxed),
        atomic_load_explicit(&executor->items, memory_order_relaxed),
        atomic_load_explicit(&executor->succeeded_items, memory_order_relaxed),
        atomic_load_explicit(&executor->failed_items, memory_order_relaxed),
        atomic_load_explicit(&executor->input_bytes, memory_order_relaxed)
    };
    return MOSAIC_OK;
}

mosaic_status mosaic_executor_reset_metrics(mosaic_executor *executor) {
    if (!executor) return MOSAIC_ERROR_INVALID_ARGUMENT;
    atomic_store_explicit(&executor->batches, 0u, memory_order_relaxed);
    atomic_store_explicit(&executor->items, 0u, memory_order_relaxed);
    atomic_store_explicit(&executor->succeeded_items, 0u, memory_order_relaxed);
    atomic_store_explicit(&executor->failed_items, 0u, memory_order_relaxed);
    atomic_store_explicit(&executor->input_bytes, 0u, memory_order_relaxed);
    return MOSAIC_OK;
}

void mosaic_executor_free(mosaic_executor *executor) {
    if (!executor) return;
    if (mosaic_mutex_lock(&executor->mutex)) {
        executor->stopping = 1;
        mosaic_cond_broadcast(&executor->not_empty);
        mosaic_cond_broadcast(&executor->not_full);
        mosaic_mutex_unlock(&executor->mutex);
    }
    for (size_t i = 0; i < executor->config.worker_count; ++i) mosaic_thread_join(executor->workers[i]);
    mosaic_cond_destroy(&executor->not_full); mosaic_cond_destroy(&executor->not_empty); mosaic_mutex_destroy(&executor->mutex);
    free(executor->queue); free(executor->workers); free(executor);
}
