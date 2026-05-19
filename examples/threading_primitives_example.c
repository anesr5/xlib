#include <xlib/thread.h>

#include <stdio.h>

enum {
    WORKER_COUNT = 4,
    INCREMENTS_PER_WORKER = 1000
};

struct shared_state {
    x_mutex_t *mutex;
    x_condition_t *condition;
    x_semaphore_t *semaphore;
    x_tls_key_t *tls_key;
    int counter;
    int ready_workers;
};

struct pool_state {
    x_mutex_t *mutex;
    x_condition_t *condition;
    int count;
};

struct barrier_state {
    x_barrier_t *barrier;
    int *counter;
};

static int once_counter;
static x_once_t once_control = X_ONCE_INIT;

static int primitive_worker(void *data)
{
    struct shared_state *state = (struct shared_state *)data;
    int local_value = 7;
    int i;

    if (x_tls_set(state->tls_key, &local_value) != 0) {
        return 1;
    }

    if (x_semaphore_wait(state->semaphore) != 0) {
        return 1;
    }

    for (i = 0; i < INCREMENTS_PER_WORKER; ++i) {
        if (x_mutex_lock(state->mutex) != 0) {
            return 1;
        }

        ++state->counter;

        if (x_mutex_unlock(state->mutex) != 0) {
            return 1;
        }
    }

    if (x_mutex_lock(state->mutex) != 0) {
        return 1;
    }

    ++state->ready_workers;
    if (x_condition_signal(state->condition) != 0) {
        x_mutex_unlock(state->mutex);
        return 1;
    }

    if (x_mutex_unlock(state->mutex) != 0) {
        return 1;
    }

    return x_tls_get(state->tls_key) == &local_value ? 0 : 1;
}

static void once_callback(void *data)
{
    int *value = (int *)data;
    ++*value;
}

static void pool_task(void *data)
{
    struct pool_state *state = (struct pool_state *)data;
    x_mutex_lock(state->mutex);
    ++state->count;
    x_condition_signal(state->condition);
    x_mutex_unlock(state->mutex);
}

static int barrier_worker(void *data)
{
    struct barrier_state *state = (struct barrier_state *)data;
    x_barrier_wait(state->barrier);
    ++*state->counter;
    return 0;
}

static int simple_worker(void *data)
{
    (void)data;
    return 0;
}

static int check_advanced_primitives(void)
{
    x_rwlock_t *rwlock;
    x_barrier_t *barrier;
    x_thread_t *thread;
    x_thread_pool_t *pool;
    x_thread_options_t options;
    struct barrier_state barrier_state;
    struct pool_state pool_state;
    int barrier_counter = 0;
    int result = 1;

    if (x_rwlock_create(&rwlock) != 0) {
        return 1;
    }
    if (x_rwlock_read_lock(rwlock) != 0 || x_rwlock_read_unlock(rwlock) != 0
        || x_rwlock_write_lock(rwlock) != 0 || x_rwlock_write_unlock(rwlock) != 0) {
        x_rwlock_destroy(rwlock);
        return 1;
    }
    x_rwlock_destroy(rwlock);

    if (x_barrier_create(&barrier, 2U) != 0) {
        return 1;
    }
    barrier_state.barrier = barrier;
    barrier_state.counter = &barrier_counter;
    if (x_thread_create(&thread, barrier_worker, &barrier_state) != 0) {
        x_barrier_destroy(barrier);
        return 1;
    }
    x_barrier_wait(barrier);
    if (x_thread_join(thread, &result) != 0 || result != 0) {
        x_thread_destroy(thread);
        x_barrier_destroy(barrier);
        return 1;
    }
    x_thread_destroy(thread);
    x_barrier_destroy(barrier);
    if (barrier_counter != 1) {
        return 1;
    }

    if (x_once(&once_control, once_callback, &once_counter) != 0
        || x_once(&once_control, once_callback, &once_counter) != 0
        || once_counter != 1) {
        return 1;
    }

    if (x_mutex_create(&pool_state.mutex) != 0 || x_condition_create(&pool_state.condition) != 0) {
        return 1;
    }
    pool_state.count = 0;
    if (x_thread_pool_create(&pool, 2U) != 0
        || x_thread_pool_submit(pool, pool_task, &pool_state) != 0
        || x_thread_pool_submit(pool, pool_task, &pool_state) != 0) {
        return 1;
    }
    x_mutex_lock(pool_state.mutex);
    while (pool_state.count < 2) {
        x_condition_wait(pool_state.condition, pool_state.mutex);
    }
    x_mutex_unlock(pool_state.mutex);
    x_thread_pool_destroy(pool);
    x_condition_destroy(pool_state.condition);
    x_mutex_destroy(pool_state.mutex);

    options.stack_size = 0U;
    if (x_thread_create_with_options(&thread, simple_worker, NULL, &options) != 0) {
        return 1;
    }
    x_thread_join(thread, NULL);
    x_thread_destroy(thread);

    return 0;
}

static int check_recursive_mutex(void)
{
    x_mutex_t *mutex;

    if (x_recursive_mutex_create(&mutex) != 0) {
        return 1;
    }

    if (x_mutex_lock(mutex) != 0 || x_mutex_lock(mutex) != 0) {
        x_mutex_destroy(mutex);
        return 1;
    }

    if (x_mutex_unlock(mutex) != 0 || x_mutex_unlock(mutex) != 0) {
        x_mutex_destroy(mutex);
        return 1;
    }

    x_mutex_destroy(mutex);
    return 0;
}

int main(void)
{
    struct shared_state state;
    x_thread_t *threads[WORKER_COUNT];
    int i;

    state.counter = 0;
    state.ready_workers = 0;

    if (x_mutex_create(&state.mutex) != 0) {
        return 1;
    }

    if (x_condition_create(&state.condition) != 0) {
        x_mutex_destroy(state.mutex);
        return 1;
    }

    if (x_semaphore_create(&state.semaphore, 0) != 0) {
        x_condition_destroy(state.condition);
        x_mutex_destroy(state.mutex);
        return 1;
    }

    if (x_tls_key_create(&state.tls_key) != 0) {
        x_semaphore_destroy(state.semaphore);
        x_condition_destroy(state.condition);
        x_mutex_destroy(state.mutex);
        return 1;
    }

    if (check_recursive_mutex() != 0 || check_advanced_primitives() != 0) {
        return 1;
    }

    for (i = 0; i < WORKER_COUNT; ++i) {
        if (x_thread_create(&threads[i], primitive_worker, &state) != 0) {
            return 1;
        }
    }

    for (i = 0; i < WORKER_COUNT; ++i) {
        if (x_semaphore_post(state.semaphore) != 0) {
            return 1;
        }
    }

    if (x_mutex_lock(state.mutex) != 0) {
        return 1;
    }

    while (state.ready_workers < WORKER_COUNT) {
        if (x_condition_wait(state.condition, state.mutex) != 0) {
            x_mutex_unlock(state.mutex);
            return 1;
        }
    }

    if (x_mutex_unlock(state.mutex) != 0) {
        return 1;
    }

    for (i = 0; i < WORKER_COUNT; ++i) {
        int result = 1;
        if (x_thread_join(threads[i], &result) != 0 || result != 0) {
            return 1;
        }
        x_thread_destroy(threads[i]);
    }

    x_tls_key_destroy(state.tls_key);
    x_semaphore_destroy(state.semaphore);
    x_condition_destroy(state.condition);
    x_mutex_destroy(state.mutex);

    if (state.counter != WORKER_COUNT * INCREMENTS_PER_WORKER) {
        return 1;
    }

    printf("Threading primitives example passed.\n");
    return 0;
}
