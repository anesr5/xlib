#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <xlib/ipc.h>

#include <errno.h>
#include <limits.h>
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
