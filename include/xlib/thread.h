#ifndef XLIB_THREAD_H
#define XLIB_THREAD_H

#include <stddef.h>

#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct x_thread x_thread_t;
typedef struct x_mutex x_mutex_t;
typedef struct x_condition x_condition_t;
typedef struct x_semaphore x_semaphore_t;
typedef struct x_tls_key x_tls_key_t;
typedef struct x_rwlock x_rwlock_t;
typedef struct x_barrier x_barrier_t;
typedef struct x_thread_pool x_thread_pool_t;

typedef int (*x_thread_fn)(void *data);
typedef void (*x_once_fn)(void *data);
typedef void (*x_thread_pool_task_fn)(void *data);

typedef struct x_thread_options {
    size_t stack_size;
} x_thread_options_t;

typedef struct x_once {
    volatile int state;
    void *reserved;
} x_once_t;

#define X_ONCE_INIT { 0, 0 }

/*
 * Creates a joinable native thread that runs function(data).
 *
 * Returns 0 on success or an errno-style error code on failure.
 * Destroy a successfully created thread with x_thread_destroy after joining it.
 */
XLIB_API int x_thread_create(x_thread_t **thread, x_thread_fn function, void *data);
XLIB_API int x_thread_create_with_options(
    x_thread_t **thread,
    x_thread_fn function,
    void *data,
    const x_thread_options_t *options);
XLIB_API int x_thread_set_affinity(x_thread_t *thread, unsigned int cpu_index);
XLIB_API int x_thread_current_set_affinity(unsigned int cpu_index);

/*
 * Blocks until thread exits. If result is not NULL, receives the integer
 * returned by the thread function.
 */
XLIB_API int x_thread_join(x_thread_t *thread, int *result);

XLIB_API void x_thread_destroy(x_thread_t *thread);

XLIB_API int x_thread_sleep_ms(unsigned int milliseconds);

/*
 * Creates a non-recursive mutex. Lock it with x_mutex_lock and release it with
 * x_mutex_unlock from the same thread.
 */
XLIB_API int x_mutex_create(x_mutex_t **mutex);

/*
 * Creates a recursive mutex that can be locked more than once by the same
 * thread. Each successful lock must be paired with an unlock.
 */
XLIB_API int x_recursive_mutex_create(x_mutex_t **mutex);
XLIB_API int x_mutex_lock(x_mutex_t *mutex);
XLIB_API int x_mutex_try_lock(x_mutex_t *mutex);
XLIB_API int x_mutex_unlock(x_mutex_t *mutex);
XLIB_API void x_mutex_destroy(x_mutex_t *mutex);

XLIB_API int x_condition_create(x_condition_t **condition);

/*
 * Atomically releases mutex and waits until condition is signaled, then
 * re-acquires mutex before returning. Callers should wait in a predicate loop.
 */
XLIB_API int x_condition_wait(x_condition_t *condition, x_mutex_t *mutex);
XLIB_API int x_condition_signal(x_condition_t *condition);
XLIB_API int x_condition_broadcast(x_condition_t *condition);
XLIB_API void x_condition_destroy(x_condition_t *condition);

/*
 * Creates a counting semaphore with initial_count available permits.
 */
XLIB_API int x_semaphore_create(x_semaphore_t **semaphore, unsigned int initial_count);
XLIB_API int x_semaphore_wait(x_semaphore_t *semaphore);
XLIB_API int x_semaphore_post(x_semaphore_t *semaphore);
XLIB_API void x_semaphore_destroy(x_semaphore_t *semaphore);

/*
 * Creates a thread-local storage key. Values are initialized to NULL for each
 * thread and are not destroyed automatically.
 */
XLIB_API int x_tls_key_create(x_tls_key_t **key);
XLIB_API int x_tls_set(x_tls_key_t *key, void *value);
XLIB_API void *x_tls_get(x_tls_key_t *key);
XLIB_API void x_tls_key_destroy(x_tls_key_t *key);

XLIB_API int x_rwlock_create(x_rwlock_t **lock);
XLIB_API int x_rwlock_read_lock(x_rwlock_t *lock);
XLIB_API int x_rwlock_try_read_lock(x_rwlock_t *lock);
XLIB_API int x_rwlock_read_unlock(x_rwlock_t *lock);
XLIB_API int x_rwlock_write_lock(x_rwlock_t *lock);
XLIB_API int x_rwlock_try_write_lock(x_rwlock_t *lock);
XLIB_API int x_rwlock_write_unlock(x_rwlock_t *lock);
XLIB_API void x_rwlock_destroy(x_rwlock_t *lock);

XLIB_API int x_barrier_create(x_barrier_t **barrier, unsigned int count);
XLIB_API int x_barrier_wait(x_barrier_t *barrier);
XLIB_API void x_barrier_destroy(x_barrier_t *barrier);

XLIB_API int x_once(x_once_t *once, x_once_fn function, void *data);

XLIB_API int x_thread_pool_create(x_thread_pool_t **pool, unsigned int worker_count);
XLIB_API int x_thread_pool_submit(x_thread_pool_t *pool, x_thread_pool_task_fn function, void *data);
XLIB_API void x_thread_pool_destroy(x_thread_pool_t *pool);

#ifdef __cplusplus
}
#endif

#endif
