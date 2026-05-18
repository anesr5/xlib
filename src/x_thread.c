#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <xlib/thread.h>
#include <xlib/time.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
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
        0,
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
        int error = pthread_create(&created->handle, NULL, x_thread_entry, created);
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
