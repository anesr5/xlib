#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <xlib/thread.h>
#include <xlib/time.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#ifndef ENOSYS
#define ENOSYS ENOTSUP
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#ifdef __linux__
#include <sched.h>
#endif
#endif

struct x_thread {
    x_thread_fn function;
    void *data;
    int result;
    int joined;

#ifdef _WIN32
    HANDLE handle;
    DWORD id;
#else
    pthread_t handle;
#endif
};

struct x_mutex {
    int recursive;

#ifdef _WIN32
    SRWLOCK lock;
    CRITICAL_SECTION recursive_lock;
#else
    pthread_mutex_t handle;
#endif
};

struct x_condition {
#ifdef _WIN32
    CONDITION_VARIABLE handle;
#else
    pthread_cond_t handle;
#endif
};

struct x_semaphore {
    unsigned int count;

#ifdef _WIN32
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE condition;
#else
    pthread_mutex_t mutex;
    pthread_cond_t condition;
#endif
};

struct x_tls_key {
#ifdef _WIN32
    DWORD handle;
#else
    pthread_key_t handle;
#endif
};

struct x_rwlock {
#ifdef _WIN32
    SRWLOCK handle;
#else
    pthread_rwlock_t handle;
#endif
};

struct x_barrier {
    unsigned int count;
    unsigned int waiting;
    unsigned int generation;
#ifdef _WIN32
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE condition;
#else
    pthread_mutex_t mutex;
    pthread_cond_t condition;
#endif
};

struct x_thread_pool_task {
    x_thread_pool_task_fn function;
    void *data;
    struct x_thread_pool_task *next;
};

struct x_thread_pool {
    x_thread_t **workers;
    unsigned int worker_count;
    struct x_thread_pool_task *head;
    struct x_thread_pool_task *tail;
    int stopping;
#ifdef _WIN32
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE condition;
#else
    pthread_mutex_t mutex;
    pthread_cond_t condition;
#endif
};

#ifdef _WIN32
static int x_thread_error_from_windows(DWORD error)
{
    switch (error) {
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return ENOMEM;
    case ERROR_INVALID_PARAMETER:
        return EINVAL;
    default:
        return EAGAIN;
    }
}

static int x_thread_error_from_wait_result(BOOL result)
{
    return result ? 0 : EINVAL;
}

static DWORD WINAPI x_thread_entry(LPVOID parameter)
{
    x_thread_t *thread = (x_thread_t *)parameter;
    thread->result = thread->function(thread->data);
    return (DWORD)thread->result;
}
#else
static void *x_thread_entry(void *parameter)
{
    x_thread_t *thread = (x_thread_t *)parameter;
    thread->result = thread->function(thread->data);
    return NULL;
}
#endif

static int x_mutex_create_with_type(x_mutex_t **mutex, int recursive)
{
    x_mutex_t *created;

    if (mutex == NULL) {
        return EINVAL;
    }

    *mutex = NULL;

    created = (x_mutex_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

    created->recursive = recursive;

#ifdef _WIN32
    if (recursive) {
        InitializeCriticalSection(&created->recursive_lock);
    } else {
        InitializeSRWLock(&created->lock);
    }
#else
    {
        pthread_mutexattr_t attr;
        int error = pthread_mutexattr_init(&attr);
        if (error != 0) {
            free(created);
            return error;
        }

        if (recursive) {
            error = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
            if (error != 0) {
                pthread_mutexattr_destroy(&attr);
                free(created);
                return error;
            }
        }

        error = pthread_mutex_init(&created->handle, &attr);
        pthread_mutexattr_destroy(&attr);
        if (error != 0) {
            free(created);
            return error;
        }
    }
#endif

    *mutex = created;
    return 0;
}

int x_thread_create(x_thread_t **thread, x_thread_fn function, void *data)
{
    return x_thread_create_with_options(thread, function, data, NULL);
}

int x_thread_create_with_options(
    x_thread_t **thread,
    x_thread_fn function,
    void *data,
    const x_thread_options_t *options)
{
    x_thread_t *created;

    if (thread == NULL || function == NULL) {
        return EINVAL;
    }

    *thread = NULL;

    created = (x_thread_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

    created->function = function;
    created->data = data;

#ifdef _WIN32
    created->handle = CreateThread(
        NULL,
        options != NULL ? options->stack_size : 0,
        x_thread_entry,
        created,
        0,
        &created->id);

    if (created->handle == NULL) {
        int error = x_thread_error_from_windows(GetLastError());
        free(created);
        return error;
    }
#else
    {
        pthread_attr_t attr;
        pthread_attr_t *attr_ptr = NULL;
        int error = 0;
        if (options != NULL && options->stack_size != 0U) {
            error = pthread_attr_init(&attr);
            if (error != 0) {
                free(created);
                return error;
            }
            attr_ptr = &attr;
            error = pthread_attr_setstacksize(&attr, options->stack_size);
            if (error != 0) {
                pthread_attr_destroy(&attr);
                free(created);
                return error;
            }
        }
        error = pthread_create(&created->handle, attr_ptr, x_thread_entry, created);
        if (attr_ptr != NULL) {
            pthread_attr_destroy(attr_ptr);
        }
        if (error != 0) {
            free(created);
            return error;
        }
    }
#endif

    *thread = created;
    return 0;
}

int x_thread_join(x_thread_t *thread, int *result)
{
    if (thread == NULL || thread->joined) {
        return EINVAL;
    }

#ifdef _WIN32
    if (WaitForSingleObject(thread->handle, INFINITE) != WAIT_OBJECT_0) {
        return EINVAL;
    }
#else
    {
        int error = pthread_join(thread->handle, NULL);
        if (error != 0) {
            return error;
        }
    }
#endif

    thread->joined = 1;
    if (result != NULL) {
        *result = thread->result;
    }

    return 0;
}

void x_thread_destroy(x_thread_t *thread)
{
    if (thread == NULL) {
        return;
    }

#ifdef _WIN32
    CloseHandle(thread->handle);
#else
    if (!thread->joined) {
        pthread_detach(thread->handle);
    }
#endif

    free(thread);
}

int x_thread_sleep_ms(unsigned int milliseconds)
{
    return x_time_sleep_ms((uint64_t)milliseconds);
}

int x_mutex_create(x_mutex_t **mutex)
{
    return x_mutex_create_with_type(mutex, 0);
}

int x_recursive_mutex_create(x_mutex_t **mutex)
{
    return x_mutex_create_with_type(mutex, 1);
}

int x_mutex_lock(x_mutex_t *mutex)
{
    if (mutex == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    if (mutex->recursive) {
        EnterCriticalSection(&mutex->recursive_lock);
    } else {
        AcquireSRWLockExclusive(&mutex->lock);
    }
    return 0;
#else
    return pthread_mutex_lock(&mutex->handle);
#endif
}

int x_mutex_try_lock(x_mutex_t *mutex)
{
    if (mutex == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    if (mutex->recursive) {
        return TryEnterCriticalSection(&mutex->recursive_lock) ? 0 : EBUSY;
    }

    return TryAcquireSRWLockExclusive(&mutex->lock) ? 0 : EBUSY;
#else
    return pthread_mutex_trylock(&mutex->handle);
#endif
}

int x_mutex_unlock(x_mutex_t *mutex)
{
    if (mutex == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    if (mutex->recursive) {
        LeaveCriticalSection(&mutex->recursive_lock);
    } else {
        ReleaseSRWLockExclusive(&mutex->lock);
    }
    return 0;
#else
    return pthread_mutex_unlock(&mutex->handle);
#endif
}

void x_mutex_destroy(x_mutex_t *mutex)
{
    if (mutex == NULL) {
        return;
    }

#ifdef _WIN32
    if (mutex->recursive) {
        DeleteCriticalSection(&mutex->recursive_lock);
    }
#else
    pthread_mutex_destroy(&mutex->handle);
#endif

    free(mutex);
}

int x_condition_create(x_condition_t **condition)
{
    x_condition_t *created;

    if (condition == NULL) {
        return EINVAL;
    }

    *condition = NULL;

    created = (x_condition_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

#ifdef _WIN32
    InitializeConditionVariable(&created->handle);
#else
    {
        int error = pthread_cond_init(&created->handle, NULL);
        if (error != 0) {
            free(created);
            return error;
        }
    }
#endif

    *condition = created;
    return 0;
}

int x_condition_wait(x_condition_t *condition, x_mutex_t *mutex)
{
    if (condition == NULL || mutex == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    if (mutex->recursive) {
        return x_thread_error_from_wait_result(
            SleepConditionVariableCS(&condition->handle, &mutex->recursive_lock, INFINITE));
    }

    return x_thread_error_from_wait_result(
        SleepConditionVariableSRW(&condition->handle, &mutex->lock, INFINITE, 0));
#else
    return pthread_cond_wait(&condition->handle, &mutex->handle);
#endif
}

int x_condition_signal(x_condition_t *condition)
{
    if (condition == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    WakeConditionVariable(&condition->handle);
    return 0;
#else
    return pthread_cond_signal(&condition->handle);
#endif
}

int x_condition_broadcast(x_condition_t *condition)
{
    if (condition == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    WakeAllConditionVariable(&condition->handle);
    return 0;
#else
    return pthread_cond_broadcast(&condition->handle);
#endif
}

void x_condition_destroy(x_condition_t *condition)
{
    if (condition == NULL) {
        return;
    }

#ifndef _WIN32
    pthread_cond_destroy(&condition->handle);
#endif

    free(condition);
}

int x_semaphore_create(x_semaphore_t **semaphore, unsigned int initial_count)
{
    x_semaphore_t *created;

    if (semaphore == NULL) {
        return EINVAL;
    }

    *semaphore = NULL;

    created = (x_semaphore_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

    created->count = initial_count;

#ifdef _WIN32
    InitializeCriticalSection(&created->mutex);
    InitializeConditionVariable(&created->condition);
#else
    {
        int error = pthread_mutex_init(&created->mutex, NULL);
        if (error != 0) {
            free(created);
            return error;
        }

        error = pthread_cond_init(&created->condition, NULL);
        if (error != 0) {
            pthread_mutex_destroy(&created->mutex);
            free(created);
            return error;
        }
    }
#endif

    *semaphore = created;
    return 0;
}

int x_semaphore_wait(x_semaphore_t *semaphore)
{
    if (semaphore == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    EnterCriticalSection(&semaphore->mutex);
    while (semaphore->count == 0U) {
        if (!SleepConditionVariableCS(&semaphore->condition, &semaphore->mutex, INFINITE)) {
            LeaveCriticalSection(&semaphore->mutex);
            return EINVAL;
        }
    }

    --semaphore->count;
    LeaveCriticalSection(&semaphore->mutex);
    return 0;
#else
    {
        int error = pthread_mutex_lock(&semaphore->mutex);
        if (error != 0) {
            return error;
        }

        while (semaphore->count == 0U) {
            error = pthread_cond_wait(&semaphore->condition, &semaphore->mutex);
            if (error != 0) {
                pthread_mutex_unlock(&semaphore->mutex);
                return error;
            }
        }

        --semaphore->count;
        error = pthread_mutex_unlock(&semaphore->mutex);
        return error;
    }
#endif
}

int x_semaphore_post(x_semaphore_t *semaphore)
{
    if (semaphore == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    EnterCriticalSection(&semaphore->mutex);
    if (semaphore->count == UINT_MAX) {
        LeaveCriticalSection(&semaphore->mutex);
        return EOVERFLOW;
    }

    ++semaphore->count;
    WakeConditionVariable(&semaphore->condition);
    LeaveCriticalSection(&semaphore->mutex);
    return 0;
#else
    {
        int error = pthread_mutex_lock(&semaphore->mutex);
        if (error != 0) {
            return error;
        }

        if (semaphore->count == UINT_MAX) {
            pthread_mutex_unlock(&semaphore->mutex);
            return EOVERFLOW;
        }

        ++semaphore->count;
        error = pthread_cond_signal(&semaphore->condition);
        if (error != 0) {
            pthread_mutex_unlock(&semaphore->mutex);
            return error;
        }

        return pthread_mutex_unlock(&semaphore->mutex);
    }
#endif
}

void x_semaphore_destroy(x_semaphore_t *semaphore)
{
    if (semaphore == NULL) {
        return;
    }

#ifdef _WIN32
    DeleteCriticalSection(&semaphore->mutex);
#else
    pthread_cond_destroy(&semaphore->condition);
    pthread_mutex_destroy(&semaphore->mutex);
#endif

    free(semaphore);
}

int x_tls_key_create(x_tls_key_t **key)
{
    x_tls_key_t *created;

    if (key == NULL) {
        return EINVAL;
    }

    *key = NULL;

    created = (x_tls_key_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

#ifdef _WIN32
    created->handle = TlsAlloc();
    if (created->handle == TLS_OUT_OF_INDEXES) {
        free(created);
        return EAGAIN;
    }
#else
    {
        int error = pthread_key_create(&created->handle, NULL);
        if (error != 0) {
            free(created);
            return error;
        }
    }
#endif

    *key = created;
    return 0;
}

int x_tls_set(x_tls_key_t *key, void *value)
{
    if (key == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    return TlsSetValue(key->handle, value) ? 0 : EINVAL;
#else
    return pthread_setspecific(key->handle, value);
#endif
}

void *x_tls_get(x_tls_key_t *key)
{
    if (key == NULL) {
        return NULL;
    }

#ifdef _WIN32
    return TlsGetValue(key->handle);
#else
    return pthread_getspecific(key->handle);
#endif
}

void x_tls_key_destroy(x_tls_key_t *key)
{
    if (key == NULL) {
        return;
    }

#ifdef _WIN32
    TlsFree(key->handle);
#else
    pthread_key_delete(key->handle);
#endif

    free(key);
}

int x_thread_set_affinity(x_thread_t *thread, unsigned int cpu_index)
{
    if (thread == NULL) {
        return EINVAL;
    }
#ifdef _WIN32
    return SetThreadAffinityMask(thread->handle, ((DWORD_PTR)1) << cpu_index) != 0 ? 0 : EINVAL;
#elif defined(__linux__)
    {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(cpu_index, &set);
        return pthread_setaffinity_np(thread->handle, sizeof(set), &set);
    }
#else
    (void)cpu_index;
    return ENOSYS;
#endif
}

int x_thread_current_set_affinity(unsigned int cpu_index)
{
#ifdef _WIN32
    return SetThreadAffinityMask(GetCurrentThread(), ((DWORD_PTR)1) << cpu_index) != 0 ? 0 : EINVAL;
#elif defined(__linux__)
    {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(cpu_index, &set);
        return pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
    }
#else
    (void)cpu_index;
    return ENOSYS;
#endif
}

int x_rwlock_create(x_rwlock_t **lock)
{
    x_rwlock_t *created;
    if (lock == NULL) return EINVAL;
    *lock = NULL;
    created = (x_rwlock_t *)calloc(1, sizeof(*created));
    if (created == NULL) return ENOMEM;
#ifdef _WIN32
    InitializeSRWLock(&created->handle);
#else
    { int error = pthread_rwlock_init(&created->handle, NULL); if (error != 0) { free(created); return error; } }
#endif
    *lock = created;
    return 0;
}

int x_rwlock_read_lock(x_rwlock_t *lock)
{
    if (lock == NULL) return EINVAL;
#ifdef _WIN32
    AcquireSRWLockShared(&lock->handle); return 0;
#else
    return pthread_rwlock_rdlock(&lock->handle);
#endif
}

int x_rwlock_try_read_lock(x_rwlock_t *lock)
{
    if (lock == NULL) return EINVAL;
#ifdef _WIN32
    return TryAcquireSRWLockShared(&lock->handle) ? 0 : EBUSY;
#else
    return pthread_rwlock_tryrdlock(&lock->handle);
#endif
}

int x_rwlock_read_unlock(x_rwlock_t *lock)
{
    if (lock == NULL) return EINVAL;
#ifdef _WIN32
    ReleaseSRWLockShared(&lock->handle); return 0;
#else
    return pthread_rwlock_unlock(&lock->handle);
#endif
}

int x_rwlock_write_lock(x_rwlock_t *lock)
{
    if (lock == NULL) return EINVAL;
#ifdef _WIN32
    AcquireSRWLockExclusive(&lock->handle); return 0;
#else
    return pthread_rwlock_wrlock(&lock->handle);
#endif
}

int x_rwlock_try_write_lock(x_rwlock_t *lock)
{
    if (lock == NULL) return EINVAL;
#ifdef _WIN32
    return TryAcquireSRWLockExclusive(&lock->handle) ? 0 : EBUSY;
#else
    return pthread_rwlock_trywrlock(&lock->handle);
#endif
}

int x_rwlock_write_unlock(x_rwlock_t *lock)
{
    if (lock == NULL) return EINVAL;
#ifdef _WIN32
    ReleaseSRWLockExclusive(&lock->handle); return 0;
#else
    return pthread_rwlock_unlock(&lock->handle);
#endif
}

void x_rwlock_destroy(x_rwlock_t *lock)
{
    if (lock == NULL) return;
#ifndef _WIN32
    pthread_rwlock_destroy(&lock->handle);
#endif
    free(lock);
}

int x_barrier_create(x_barrier_t **barrier, unsigned int count)
{
    x_barrier_t *created;
    if (barrier == NULL || count == 0U) return EINVAL;
    *barrier = NULL;
    created = (x_barrier_t *)calloc(1, sizeof(*created));
    if (created == NULL) return ENOMEM;
    created->count = count;
#ifdef _WIN32
    InitializeCriticalSection(&created->mutex);
    InitializeConditionVariable(&created->condition);
#else
    { int error = pthread_mutex_init(&created->mutex, NULL); if (error != 0) { free(created); return error; }
      error = pthread_cond_init(&created->condition, NULL); if (error != 0) { pthread_mutex_destroy(&created->mutex); free(created); return error; } }
#endif
    *barrier = created;
    return 0;
}

int x_barrier_wait(x_barrier_t *barrier)
{
    unsigned int generation;
    if (barrier == NULL) return EINVAL;
#ifdef _WIN32
    EnterCriticalSection(&barrier->mutex);
    generation = barrier->generation;
    ++barrier->waiting;
    if (barrier->waiting == barrier->count) {
        barrier->waiting = 0U; ++barrier->generation; WakeAllConditionVariable(&barrier->condition); LeaveCriticalSection(&barrier->mutex); return 1;
    }
    while (generation == barrier->generation) SleepConditionVariableCS(&barrier->condition, &barrier->mutex, INFINITE);
    LeaveCriticalSection(&barrier->mutex);
#else
    pthread_mutex_lock(&barrier->mutex);
    generation = barrier->generation;
    ++barrier->waiting;
    if (barrier->waiting == barrier->count) {
        barrier->waiting = 0U; ++barrier->generation; pthread_cond_broadcast(&barrier->condition); pthread_mutex_unlock(&barrier->mutex); return 1;
    }
    while (generation == barrier->generation) pthread_cond_wait(&barrier->condition, &barrier->mutex);
    pthread_mutex_unlock(&barrier->mutex);
#endif
    return 0;
}

void x_barrier_destroy(x_barrier_t *barrier)
{
    if (barrier == NULL) return;
#ifdef _WIN32
    DeleteCriticalSection(&barrier->mutex);
#else
    pthread_cond_destroy(&barrier->condition); pthread_mutex_destroy(&barrier->mutex);
#endif
    free(barrier);
}

int x_once(x_once_t *once, x_once_fn function, void *data)
{
    if (once == NULL || function == NULL) return EINVAL;
#ifdef _WIN32
    while (InterlockedCompareExchange((volatile LONG *)&once->state, 1, 0) != 0) {
        if (once->state == 2) return 0;
        Sleep(0);
    }
    function(data);
    InterlockedExchange((volatile LONG *)&once->state, 2);
#else
    if (__sync_bool_compare_and_swap(&once->state, 0, 1)) {
        function(data);
        __sync_synchronize();
        once->state = 2;
    } else {
        while (once->state != 2) sched_yield();
    }
#endif
    return 0;
}

static int x_thread_pool_worker(void *data)
{
    x_thread_pool_t *pool = (x_thread_pool_t *)data;
    for (;;) {
        struct x_thread_pool_task *task;
#ifdef _WIN32
        EnterCriticalSection(&pool->mutex);
        while (!pool->stopping && pool->head == NULL) SleepConditionVariableCS(&pool->condition, &pool->mutex, INFINITE);
        if (pool->stopping && pool->head == NULL) { LeaveCriticalSection(&pool->mutex); break; }
        task = pool->head; pool->head = task->next; if (pool->head == NULL) pool->tail = NULL;
        LeaveCriticalSection(&pool->mutex);
#else
        pthread_mutex_lock(&pool->mutex);
        while (!pool->stopping && pool->head == NULL) pthread_cond_wait(&pool->condition, &pool->mutex);
        if (pool->stopping && pool->head == NULL) { pthread_mutex_unlock(&pool->mutex); break; }
        task = pool->head; pool->head = task->next; if (pool->head == NULL) pool->tail = NULL;
        pthread_mutex_unlock(&pool->mutex);
#endif
        task->function(task->data);
        free(task);
    }
    return 0;
}

int x_thread_pool_create(x_thread_pool_t **pool, unsigned int worker_count)
{
    x_thread_pool_t *created;
    unsigned int i;
    if (pool == NULL || worker_count == 0U) return EINVAL;
    *pool = NULL;
    created = (x_thread_pool_t *)calloc(1, sizeof(*created));
    if (created == NULL) return ENOMEM;
    created->worker_count = worker_count;
    created->workers = (x_thread_t **)calloc(worker_count, sizeof(*created->workers));
    if (created->workers == NULL) { free(created); return ENOMEM; }
#ifdef _WIN32
    InitializeCriticalSection(&created->mutex); InitializeConditionVariable(&created->condition);
#else
    pthread_mutex_init(&created->mutex, NULL); pthread_cond_init(&created->condition, NULL);
#endif
    for (i = 0; i < worker_count; ++i) {
        int error = x_thread_create(&created->workers[i], x_thread_pool_worker, created);
        if (error != 0) { x_thread_pool_destroy(created); return error; }
    }
    *pool = created;
    return 0;
}

int x_thread_pool_submit(x_thread_pool_t *pool, x_thread_pool_task_fn function, void *data)
{
    struct x_thread_pool_task *task;
    if (pool == NULL || function == NULL) return EINVAL;
    task = (struct x_thread_pool_task *)calloc(1, sizeof(*task));
    if (task == NULL) return ENOMEM;
    task->function = function; task->data = data;
#ifdef _WIN32
    EnterCriticalSection(&pool->mutex);
    if (pool->tail != NULL) pool->tail->next = task; else pool->head = task; pool->tail = task;
    WakeConditionVariable(&pool->condition);
    LeaveCriticalSection(&pool->mutex);
#else
    pthread_mutex_lock(&pool->mutex);
    if (pool->tail != NULL) pool->tail->next = task; else pool->head = task; pool->tail = task;
    pthread_cond_signal(&pool->condition);
    pthread_mutex_unlock(&pool->mutex);
#endif
    return 0;
}

void x_thread_pool_destroy(x_thread_pool_t *pool)
{
    unsigned int i;
    if (pool == NULL) return;
#ifdef _WIN32
    EnterCriticalSection(&pool->mutex); pool->stopping = 1; WakeAllConditionVariable(&pool->condition); LeaveCriticalSection(&pool->mutex);
#else
    pthread_mutex_lock(&pool->mutex); pool->stopping = 1; pthread_cond_broadcast(&pool->condition); pthread_mutex_unlock(&pool->mutex);
#endif
    for (i = 0; i < pool->worker_count; ++i) {
        if (pool->workers[i] != NULL) { x_thread_join(pool->workers[i], NULL); x_thread_destroy(pool->workers[i]); }
    }
    while (pool->head != NULL) { struct x_thread_pool_task *task = pool->head; pool->head = task->next; free(task); }
#ifdef _WIN32
    DeleteCriticalSection(&pool->mutex);
#else
    pthread_cond_destroy(&pool->condition); pthread_mutex_destroy(&pool->mutex);
#endif
    free(pool->workers); free(pool);
}
