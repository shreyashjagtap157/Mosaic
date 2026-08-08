#ifndef MOSAIC_THREAD_H
#define MOSAIC_THREAD_H

/* Internal portability shim. Public Mosaic ABI never exposes thread primitives. */
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <process.h>
#include <stdint.h>
#include <stdlib.h>

typedef CRITICAL_SECTION mosaic_mutex_t;
typedef CONDITION_VARIABLE mosaic_cond_t;
typedef HANDLE mosaic_thread_t;
typedef int (*mosaic_thread_fn)(void *);

typedef struct {
    mosaic_thread_fn fn;
    void *arg;
} mosaic_thread_start;

static unsigned __stdcall mosaic_thread_thunk(void *opaque) {
    mosaic_thread_start *start = (mosaic_thread_start *)opaque;
    mosaic_thread_fn fn = start->fn;
    void *arg = start->arg;
    free(start);
    return (unsigned)fn(arg);
}

static inline int mosaic_mutex_init(mosaic_mutex_t *m) { InitializeCriticalSection(m); return 1; }
static inline void mosaic_mutex_destroy(mosaic_mutex_t *m) { DeleteCriticalSection(m); }
static inline int mosaic_mutex_lock(mosaic_mutex_t *m) { EnterCriticalSection(m); return 1; }
static inline void mosaic_mutex_unlock(mosaic_mutex_t *m) { LeaveCriticalSection(m); }
static inline int mosaic_cond_init(mosaic_cond_t *c) { InitializeConditionVariable(c); return 1; }
static inline void mosaic_cond_destroy(mosaic_cond_t *c) { (void)c; }
static inline int mosaic_cond_wait(mosaic_cond_t *c, mosaic_mutex_t *m) { return SleepConditionVariableCS(c, m, INFINITE) != 0; }
static inline void mosaic_cond_signal(mosaic_cond_t *c) { WakeConditionVariable(c); }
static inline void mosaic_cond_broadcast(mosaic_cond_t *c) { WakeAllConditionVariable(c); }
static inline int mosaic_thread_create(mosaic_thread_t *thread, mosaic_thread_fn fn, void *arg) {
    mosaic_thread_start *start = (mosaic_thread_start *)malloc(sizeof *start);
    if (!start) return 0;
    start->fn = fn;
    start->arg = arg;
    uintptr_t handle = _beginthreadex(NULL, 0, mosaic_thread_thunk, start, 0, NULL);
    if (!handle) { free(start); return 0; }
    *thread = (HANDLE)handle;
    return 1;
}
static inline int mosaic_thread_join(mosaic_thread_t thread) {
    DWORD waited = WaitForSingleObject(thread, INFINITE);
    int closed = CloseHandle(thread) != 0;
    return waited == WAIT_OBJECT_0 && closed;
}

#else
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

typedef pthread_mutex_t mosaic_mutex_t;
typedef pthread_cond_t mosaic_cond_t;
typedef pthread_t mosaic_thread_t;
typedef int (*mosaic_thread_fn)(void *);

typedef struct {
    mosaic_thread_fn fn;
    void *arg;
} mosaic_thread_start;

static void *mosaic_thread_thunk(void *opaque) {
    mosaic_thread_start *start = (mosaic_thread_start *)opaque;
    mosaic_thread_fn fn = start->fn;
    void *arg = start->arg;
    free(start);
    (void)fn(arg);
    return NULL;
}

static inline int mosaic_mutex_init(mosaic_mutex_t *m) { return pthread_mutex_init(m, NULL) == 0; }
static inline void mosaic_mutex_destroy(mosaic_mutex_t *m) { (void)pthread_mutex_destroy(m); }
static inline int mosaic_mutex_lock(mosaic_mutex_t *m) { return pthread_mutex_lock(m) == 0; }
static inline void mosaic_mutex_unlock(mosaic_mutex_t *m) { (void)pthread_mutex_unlock(m); }
static inline int mosaic_cond_init(mosaic_cond_t *c) { return pthread_cond_init(c, NULL) == 0; }
static inline void mosaic_cond_destroy(mosaic_cond_t *c) { (void)pthread_cond_destroy(c); }
static inline int mosaic_cond_wait(mosaic_cond_t *c, mosaic_mutex_t *m) { return pthread_cond_wait(c, m) == 0; }
static inline void mosaic_cond_signal(mosaic_cond_t *c) { (void)pthread_cond_signal(c); }
static inline void mosaic_cond_broadcast(mosaic_cond_t *c) { (void)pthread_cond_broadcast(c); }
static inline int mosaic_thread_create(mosaic_thread_t *thread, mosaic_thread_fn fn, void *arg) {
    mosaic_thread_start *start = (mosaic_thread_start *)malloc(sizeof *start);
    if (!start) return 0;
    start->fn = fn;
    start->arg = arg;
    if (pthread_create(thread, NULL, mosaic_thread_thunk, start) != 0) { free(start); return 0; }
    return 1;
}
static inline int mosaic_thread_join(mosaic_thread_t thread) { return pthread_join(thread, NULL) == 0; }
#endif

#endif
