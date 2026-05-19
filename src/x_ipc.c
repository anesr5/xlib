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

#include <xlib/ipc.h>

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

struct x_shared_memory {
    void *data;
    size_t size;

#ifdef _WIN32
    HANDLE mapping;
#endif
};

struct x_named_semaphore {
#ifdef _WIN32
    HANDLE handle;
#else
    sem_t *handle;
#endif
};

struct x_named_mutex {
#ifdef _WIN32
    HANDLE handle;
#else
    sem_t *handle;
#endif
};

struct x_message_queue_header {
    size_t capacity;
    size_t message_size;
    size_t head;
    size_t tail;
};

struct x_message_queue {
    char name[128];
    x_shared_memory_t *memory;
    x_named_mutex_t *mutex;
    x_named_semaphore_t *items;
    x_named_semaphore_t *slots;
};

#ifdef _WIN32
static int x_ipc_error_from_windows(DWORD error)
{
    switch (error) {
    case ERROR_SUCCESS:
        return 0;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return ENOENT;
    case ERROR_ALREADY_EXISTS:
        return EEXIST;
    case ERROR_ACCESS_DENIED:
        return EACCES;
    case ERROR_INVALID_PARAMETER:
    case ERROR_INVALID_NAME:
        return EINVAL;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return ENOMEM;
    default:
        return EIO;
    }
}
#else
/*
 * POSIX shared memory names must begin with a leading slash and contain no
 * further slashes.  This helper prepends one when the caller omits it.
 */
static int x_ipc_make_posix_name(const char *name, char *buffer, size_t buffer_size)
{
    size_t length;

    if (name == NULL || buffer == NULL || buffer_size == 0U) {
        return EINVAL;
    }

    if (name[0] == '/') {
        length = strlen(name);
        if (length + 1U > buffer_size) {
            return ENAMETOOLONG;
        }
        memcpy(buffer, name, length + 1U);
    } else {
        length = strlen(name);
        if (length + 2U > buffer_size) {
            return ENAMETOOLONG;
        }
        buffer[0] = '/';
        memcpy(buffer + 1U, name, length + 1U);
    }

    return 0;
}
#endif

int x_shared_memory_create(x_shared_memory_t **shm, const char *name, size_t size)
{
    x_shared_memory_t *created;

    if (shm == NULL || name == NULL || size == 0U) {
        return EINVAL;
    }

    *shm = NULL;

    created = (x_shared_memory_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

    created->size = size;

#ifdef _WIN32
    {
        DWORD size_high;
        DWORD size_low;

        if (size > (size_t)0xffffffffffffffffULL) {
            free(created);
            return EOVERFLOW;
        }

        size_high = (DWORD)((ULONGLONG)size >> 32);
        size_low  = (DWORD)((ULONGLONG)size & 0xffffffffUL);

        created->mapping = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            size_high,
            size_low,
            name);

        if (created->mapping == NULL) {
            int error = x_ipc_error_from_windows(GetLastError());
            free(created);
            return error;
        }

        created->data = MapViewOfFile(created->mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (created->data == NULL) {
            int error = x_ipc_error_from_windows(GetLastError());
            CloseHandle(created->mapping);
            free(created);
            return error;
        }
    }
#else
    {
        char posix_name[256];
        int fd;
        int error = x_ipc_make_posix_name(name, posix_name, sizeof(posix_name));
        if (error != 0) {
            free(created);
            return error;
        }

        fd = shm_open(posix_name, O_RDWR | O_CREAT | O_TRUNC, 0600);
        if (fd < 0) {
            error = errno;
            free(created);
            return error;
        }

        if (ftruncate(fd, (off_t)size) != 0) {
            error = errno;
            shm_unlink(posix_name);
            close(fd);
            free(created);
            return error;
        }

        created->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);

        if (created->data == MAP_FAILED) {
            error = errno;
            shm_unlink(posix_name);
            free(created);
            return error;
        }
    }
#endif

    *shm = created;
    return 0;
}

int x_shared_memory_open(x_shared_memory_t **shm, const char *name)
{
    x_shared_memory_t *created;

    if (shm == NULL || name == NULL) {
        return EINVAL;
    }

    *shm = NULL;

    created = (x_shared_memory_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

#ifdef _WIN32
    {
        MEMORY_BASIC_INFORMATION info;

        created->mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
        if (created->mapping == NULL) {
            int error = x_ipc_error_from_windows(GetLastError());
            free(created);
            return error;
        }

        created->data = MapViewOfFile(created->mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (created->data == NULL) {
            int error = x_ipc_error_from_windows(GetLastError());
            CloseHandle(created->mapping);
            free(created);
            return error;
        }

        if (VirtualQuery(created->data, &info, sizeof(info)) == 0) {
            UnmapViewOfFile(created->data);
            CloseHandle(created->mapping);
            free(created);
            return EIO;
        }

        created->size = info.RegionSize;
    }
#else
    {
        char posix_name[256];
        struct stat status;
        int fd;
        int error = x_ipc_make_posix_name(name, posix_name, sizeof(posix_name));
        if (error != 0) {
            free(created);
            return error;
        }

        fd = shm_open(posix_name, O_RDWR, 0);
        if (fd < 0) {
            error = errno;
            free(created);
            return error;
        }

        if (fstat(fd, &status) != 0) {
            error = errno;
            close(fd);
            free(created);
            return error;
        }

        if (status.st_size == 0) {
            close(fd);
            free(created);
            return EINVAL;
        }

        created->size = (size_t)status.st_size;
        created->data = mmap(NULL, created->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);

        if (created->data == MAP_FAILED) {
            error = errno;
            free(created);
            return error;
        }
    }
#endif

    *shm = created;
    return 0;
}

void *x_shared_memory_data(x_shared_memory_t *shm)
{
    return shm == NULL ? NULL : shm->data;
}

size_t x_shared_memory_size(const x_shared_memory_t *shm)
{
    return shm == NULL ? 0U : shm->size;
}

void x_shared_memory_close(x_shared_memory_t *shm)
{
    if (shm == NULL) {
        return;
    }

#ifdef _WIN32
    if (shm->data != NULL) {
        UnmapViewOfFile(shm->data);
    }

    if (shm->mapping != NULL) {
        CloseHandle(shm->mapping);
    }
#else
    if (shm->data != NULL && shm->data != MAP_FAILED) {
        munmap(shm->data, shm->size);
    }
#endif

    free(shm);
}

int x_shared_memory_unlink(const char *name)
{
    if (name == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    /*
     * Windows shared memory objects are reference-counted by the kernel and
     * disappear automatically when the last handle is closed.  There is no
     * explicit unlink step.
     */
    return 0;
#else
    {
        char posix_name[256];
        int error = x_ipc_make_posix_name(name, posix_name, sizeof(posix_name));
        if (error != 0) {
            return error;
        }

        if (shm_unlink(posix_name) != 0) {
            return errno;
        }

        return 0;
    }
#endif
}

int x_named_semaphore_create(
    x_named_semaphore_t **semaphore,
    const char *name,
    unsigned int initial_count)
{
    x_named_semaphore_t *created;

    if (semaphore == NULL || name == NULL) {
        return EINVAL;
    }

    *semaphore = NULL;

    created = (x_named_semaphore_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

#ifdef _WIN32
    created->handle = CreateSemaphoreA(NULL, (LONG)initial_count, LONG_MAX, name);
    if (created->handle == NULL) {
        int error = x_ipc_error_from_windows(GetLastError());
        free(created);
        return error;
    }
#else
    {
        char posix_name[256];
        int error = x_ipc_make_posix_name(name, posix_name, sizeof(posix_name));
        if (error != 0) {
            free(created);
            return error;
        }

        created->handle = sem_open(posix_name, O_CREAT | O_EXCL, 0600, initial_count);
        if (created->handle == SEM_FAILED) {
            error = errno;
            free(created);
            return error;
        }
    }
#endif

    *semaphore = created;
    return 0;
}

int x_named_semaphore_open(x_named_semaphore_t **semaphore, const char *name)
{
    x_named_semaphore_t *created;

    if (semaphore == NULL || name == NULL) {
        return EINVAL;
    }

    *semaphore = NULL;

    created = (x_named_semaphore_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

#ifdef _WIN32
    created->handle = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, name);
    if (created->handle == NULL) {
        int error = x_ipc_error_from_windows(GetLastError());
        free(created);
        return error;
    }
#else
    {
        char posix_name[256];
        int error = x_ipc_make_posix_name(name, posix_name, sizeof(posix_name));
        if (error != 0) {
            free(created);
            return error;
        }

        created->handle = sem_open(posix_name, 0);
        if (created->handle == SEM_FAILED) {
            error = errno;
            free(created);
            return error;
        }
    }
#endif

    *semaphore = created;
    return 0;
}

int x_named_semaphore_wait(x_named_semaphore_t *semaphore)
{
    if (semaphore == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    if (WaitForSingleObject(semaphore->handle, INFINITE) != WAIT_OBJECT_0) {
        return x_ipc_error_from_windows(GetLastError());
    }

    return 0;
#else
    while (sem_wait(semaphore->handle) != 0) {
        if (errno != EINTR) {
            return errno;
        }
    }

    return 0;
#endif
}

int x_named_semaphore_post(x_named_semaphore_t *semaphore)
{
    if (semaphore == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    if (!ReleaseSemaphore(semaphore->handle, 1, NULL)) {
        return x_ipc_error_from_windows(GetLastError());
    }

    return 0;
#else
    if (sem_post(semaphore->handle) != 0) {
        return errno;
    }

    return 0;
#endif
}

void x_named_semaphore_close(x_named_semaphore_t *semaphore)
{
    if (semaphore == NULL) {
        return;
    }

#ifdef _WIN32
    CloseHandle(semaphore->handle);
#else
    sem_close(semaphore->handle);
#endif

    free(semaphore);
}

int x_named_semaphore_unlink(const char *name)
{
    if (name == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    /*
     * Windows named semaphores are reference-counted and destroyed
     * automatically when the last handle is closed.
     */
    return 0;
#else
    {
        char posix_name[256];
        int error = x_ipc_make_posix_name(name, posix_name, sizeof(posix_name));
        if (error != 0) {
            return error;
        }

        if (sem_unlink(posix_name) != 0) {
            return errno;
        }

        return 0;
    }
#endif
}

int x_named_mutex_create(x_named_mutex_t **mutex, const char *name)
{
    x_named_mutex_t *created;

    if (mutex == NULL || name == NULL) {
        return EINVAL;
    }

    *mutex = NULL;

    created = (x_named_mutex_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

#ifdef _WIN32
    created->handle = CreateMutexA(NULL, FALSE, name);
    if (created->handle == NULL) {
        int error = x_ipc_error_from_windows(GetLastError());
        free(created);
        return error;
    }
#else
    {
        char posix_name[256];
        int error = x_ipc_make_posix_name(name, posix_name, sizeof(posix_name));
        if (error != 0) {
            free(created);
            return error;
        }

        created->handle = sem_open(posix_name, O_CREAT, 0600, 1U);
        if (created->handle == SEM_FAILED) {
            error = errno;
            free(created);
            return error;
        }
    }
#endif

    *mutex = created;
    return 0;
}

int x_named_mutex_open(x_named_mutex_t **mutex, const char *name)
{
    x_named_mutex_t *created;

    if (mutex == NULL || name == NULL) {
        return EINVAL;
    }

    *mutex = NULL;

    created = (x_named_mutex_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

#ifdef _WIN32
    created->handle = OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, name);
    if (created->handle == NULL) {
        int error = x_ipc_error_from_windows(GetLastError());
        free(created);
        return error;
    }
#else
    {
        char posix_name[256];
        int error = x_ipc_make_posix_name(name, posix_name, sizeof(posix_name));
        if (error != 0) {
            free(created);
            return error;
        }

        created->handle = sem_open(posix_name, 0);
        if (created->handle == SEM_FAILED) {
            error = errno;
            free(created);
            return error;
        }
    }
#endif

    *mutex = created;
    return 0;
}

int x_named_mutex_lock(x_named_mutex_t *mutex)
{
    if (mutex == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    {
        DWORD result = WaitForSingleObject(mutex->handle, INFINITE);
        if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED) {
            return x_ipc_error_from_windows(GetLastError());
        }
    }
#else
    while (sem_wait(mutex->handle) != 0) {
        if (errno != EINTR) {
            return errno;
        }
    }
#endif

    return 0;
}

int x_named_mutex_unlock(x_named_mutex_t *mutex)
{
    if (mutex == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    if (!ReleaseMutex(mutex->handle)) {
        return x_ipc_error_from_windows(GetLastError());
    }
#else
    if (sem_post(mutex->handle) != 0) {
        return errno;
    }
#endif

    return 0;
}

void x_named_mutex_close(x_named_mutex_t *mutex)
{
    if (mutex == NULL) {
        return;
    }

#ifdef _WIN32
    CloseHandle(mutex->handle);
#else
    sem_close(mutex->handle);
#endif

    free(mutex);
}

int x_named_mutex_unlink(const char *name)
{
    if (name == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    return 0;
#else
    {
        char posix_name[256];
        int error = x_ipc_make_posix_name(name, posix_name, sizeof(posix_name));
        if (error != 0) {
            return error;
        }

        if (sem_unlink(posix_name) != 0) {
            return errno;
        }

        return 0;
    }
#endif
}

static int x_message_queue_part_name(const char *name, const char *suffix, char *buffer, size_t buffer_size)
{
    int result;

    if (name == NULL || suffix == NULL || buffer == NULL || buffer_size == 0U) {
        return EINVAL;
    }

    result = snprintf(buffer, buffer_size, "%s_%s", name, suffix);
    if (result < 0) {
        return EIO;
    }

    if ((size_t)result >= buffer_size) {
        return ENAMETOOLONG;
    }

    return 0;
}

static void *x_message_queue_slot(x_message_queue_t *queue, struct x_message_queue_header *header, size_t index)
{
    unsigned char *base = (unsigned char *)x_shared_memory_data(queue->memory);
    return base + sizeof(*header) + (index * (sizeof(size_t) + header->message_size));
}

static void x_message_queue_cleanup(x_message_queue_t *queue)
{
    if (queue == NULL) {
        return;
    }

    if (queue->slots != NULL) {
        x_named_semaphore_close(queue->slots);
    }
    if (queue->items != NULL) {
        x_named_semaphore_close(queue->items);
    }
    if (queue->mutex != NULL) {
        x_named_mutex_close(queue->mutex);
    }
    if (queue->memory != NULL) {
        x_shared_memory_close(queue->memory);
    }

    free(queue);
}

int x_message_queue_create(
    x_message_queue_t **queue,
    const char *name,
    size_t capacity,
    size_t message_size)
{
    x_message_queue_t *created;
    struct x_message_queue_header *header;
    char mutex_name[160];
    char items_name[160];
    char slots_name[160];
    size_t total_size;
    int error;

    if (queue == NULL || name == NULL || capacity == 0U || message_size == 0U) {
        return EINVAL;
    }

    if (strlen(name) >= sizeof(created->name)) {
        return ENAMETOOLONG;
    }

    if (message_size > SIZE_MAX - sizeof(size_t)
        || capacity > (SIZE_MAX - sizeof(*header)) / (sizeof(size_t) + message_size)) {
        return EOVERFLOW;
    }

    *queue = NULL;
    total_size = sizeof(*header) + capacity * (sizeof(size_t) + message_size);

    created = (x_message_queue_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }
    strcpy(created->name, name);

    error = x_message_queue_part_name(name, "mutex", mutex_name, sizeof(mutex_name));
    if (error == 0) {
        error = x_message_queue_part_name(name, "items", items_name, sizeof(items_name));
    }
    if (error == 0) {
        error = x_message_queue_part_name(name, "slots", slots_name, sizeof(slots_name));
    }
    if (error != 0) {
        x_message_queue_cleanup(created);
        return error;
    }

    error = x_shared_memory_create(&created->memory, name, total_size);
    if (error == 0) {
        error = x_named_mutex_create(&created->mutex, mutex_name);
    }
    if (error == 0) {
        error = x_named_semaphore_create(&created->items, items_name, 0U);
    }
    if (error == 0) {
        error = x_named_semaphore_create(&created->slots, slots_name, (unsigned int)capacity);
    }
    if (error != 0) {
        x_message_queue_cleanup(created);
        return error;
    }

    header = (struct x_message_queue_header *)x_shared_memory_data(created->memory);
    header->capacity = capacity;
    header->message_size = message_size;
    header->head = 0U;
    header->tail = 0U;

    *queue = created;
    return 0;
}

int x_message_queue_open(x_message_queue_t **queue, const char *name)
{
    x_message_queue_t *created;
    char mutex_name[160];
    char items_name[160];
    char slots_name[160];
    int error;

    if (queue == NULL || name == NULL) {
        return EINVAL;
    }

    if (strlen(name) >= sizeof(created->name)) {
        return ENAMETOOLONG;
    }

    *queue = NULL;

    created = (x_message_queue_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }
    strcpy(created->name, name);

    error = x_message_queue_part_name(name, "mutex", mutex_name, sizeof(mutex_name));
    if (error == 0) {
        error = x_message_queue_part_name(name, "items", items_name, sizeof(items_name));
    }
    if (error == 0) {
        error = x_message_queue_part_name(name, "slots", slots_name, sizeof(slots_name));
    }
    if (error == 0) {
        error = x_shared_memory_open(&created->memory, name);
    }
    if (error == 0) {
        error = x_named_mutex_open(&created->mutex, mutex_name);
    }
    if (error == 0) {
        error = x_named_semaphore_open(&created->items, items_name);
    }
    if (error == 0) {
        error = x_named_semaphore_open(&created->slots, slots_name);
    }
    if (error != 0) {
        x_message_queue_cleanup(created);
        return error;
    }

    *queue = created;
    return 0;
}

int x_message_queue_send(x_message_queue_t *queue, const void *data, size_t size)
{
    struct x_message_queue_header *header;
    void *slot;
    int error;

    if (queue == NULL || data == NULL) {
        return EINVAL;
    }

    header = (struct x_message_queue_header *)x_shared_memory_data(queue->memory);
    if (size > header->message_size) {
        return EMSGSIZE;
    }

    error = x_named_semaphore_wait(queue->slots);
    if (error != 0) {
        return error;
    }
    error = x_named_mutex_lock(queue->mutex);
    if (error != 0) {
        x_named_semaphore_post(queue->slots);
        return error;
    }

    slot = x_message_queue_slot(queue, header, header->tail);
    *((size_t *)slot) = size;
    memset((unsigned char *)slot + sizeof(size_t), 0, header->message_size);
    memcpy((unsigned char *)slot + sizeof(size_t), data, size);
    header->tail = (header->tail + 1U) % header->capacity;

    error = x_named_mutex_unlock(queue->mutex);
    if (error == 0) {
        error = x_named_semaphore_post(queue->items);
    }
    return error;
}

int x_message_queue_receive(x_message_queue_t *queue, void *buffer, size_t buffer_size, size_t *size)
{
    struct x_message_queue_header *header;
    void *slot;
    size_t copied;
    int error;

    if (queue == NULL || buffer == NULL) {
        return EINVAL;
    }

    if (size != NULL) {
        *size = 0U;
    }

    header = (struct x_message_queue_header *)x_shared_memory_data(queue->memory);
    copied = 0U;

    error = x_named_semaphore_wait(queue->items);
    if (error != 0) {
        return error;
    }
    error = x_named_mutex_lock(queue->mutex);
    if (error != 0) {
        x_named_semaphore_post(queue->items);
        return error;
    }

    slot = x_message_queue_slot(queue, header, header->head);
    copied = *((size_t *)slot);
    if (copied > buffer_size) {
        copied = buffer_size;
    }
    memcpy(buffer, (unsigned char *)slot + sizeof(size_t), copied);
    header->head = (header->head + 1U) % header->capacity;

    error = x_named_mutex_unlock(queue->mutex);
    if (error == 0) {
        error = x_named_semaphore_post(queue->slots);
    }
    if (error == 0 && size != NULL) {
        *size = copied;
    }
    return error;
}

void x_message_queue_close(x_message_queue_t *queue)
{
    x_message_queue_cleanup(queue);
}

int x_message_queue_unlink(const char *name)
{
    char mutex_name[160];
    char items_name[160];
    char slots_name[160];
    int error;
    int result = 0;

    if (name == NULL) {
        return EINVAL;
    }

    error = x_message_queue_part_name(name, "mutex", mutex_name, sizeof(mutex_name));
    if (error == 0) {
        error = x_message_queue_part_name(name, "items", items_name, sizeof(items_name));
    }
    if (error == 0) {
        error = x_message_queue_part_name(name, "slots", slots_name, sizeof(slots_name));
    }
    if (error != 0) {
        return error;
    }

    error = x_shared_memory_unlink(name);
    if (error != 0 && error != ENOENT) {
        result = error;
    }
    error = x_named_mutex_unlink(mutex_name);
    if (result == 0 && error != 0 && error != ENOENT) {
        result = error;
    }
    error = x_named_semaphore_unlink(items_name);
    if (result == 0 && error != 0 && error != ENOENT) {
        result = error;
    }
    error = x_named_semaphore_unlink(slots_name);
    if (result == 0 && error != 0 && error != ENOENT) {
        result = error;
    }

    return result;
}
