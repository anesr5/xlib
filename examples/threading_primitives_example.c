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

    if (check_recursive_mutex() != 0) {
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
