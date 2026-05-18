#ifndef XLIB_THREAD_H
#define XLIB_THREAD_H

#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct x_thread x_thread_t;
typedef struct x_mutex x_mutex_t;
typedef struct x_condition x_condition_t;
typedef struct x_semaphore x_semaphore_t;
typedef struct x_tls_key x_tls_key_t;

typedef int (*x_thread_fn)(void *data);

/*
 * Creates a joinable native thread that runs function(data).
 *
 * Returns 0 on success or an errno-style error code on failure.
 * Destroy a successfully created thread with x_thread_destroy after joining it.
 */
XLIB_API int x_thread_create(x_thread_t **thread, x_thread_fn function, void *data);

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

#ifdef __cplusplus
}
#endif

#endif
